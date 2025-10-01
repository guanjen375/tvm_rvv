# my_device 兩種模式切換指南

## 📋 模式選擇

### 模式 1：LLVM Backend（RVV 交叉編譯）
**用途：** 主管要的 RVV device，x86 → Banana Pi

**設定：** `build/config.cmake`
```cmake
set(TVM_MY_DEVICE_USE_LLVM ON)
```

**流程：**
```
x86 編譯：
  rvv_test.py → Buildmy_device → LLVM backend
  → RISC-V + RVV IR → cross-compile → mobilenet.so

Banana Pi 執行：
  run.py → 載入 DSO module → 直接執行
```

---

### 模式 2：C++ Runtime（其他 device 開發）
**用途：** 開發其他 device，需要 runtime 動態編譯

**設定：** `build/config.cmake`
```cmake
set(TVM_MY_DEVICE_USE_LLVM OFF)
```

**流程：**
```
編譯：
  script → Buildmy_device → CodeGenmy_device
  → C++ source → my_device module

執行：
  第一次執行 → compile_model_dynamic
  → g++ 編譯 → .so → 緩存 → 執行
  
  後續執行 → 使用緩存 → 執行
```

---

## 🔧 切換步驟

```bash
# 1. 修改配置
cd /home/david/tvm
vim build/config.cmake
# 改 set(TVM_MY_DEVICE_USE_LLVM ON/OFF)

# 2. 重新 cmake
cd build
cmake ..

# 3. 重新編譯
make -j4  # 或 ninja -j$(nproc)
```

---

## 📝 快速參考

| 項目 | LLVM Mode (ON) | C++ Runtime (OFF) |
|------|---------------|-------------------|
| **用途** | RVV/交叉編譯 | 其他 device 開發 |
| **Codegen** | LLVM backend | CodeGenmy_device |
| **返回** | LLVM module | my_device module |
| **Runtime** | 載入 DSO | 動態編譯 C++ |
| **支援交叉編譯** | ✅ | ❌ |
| **rvv_test.py** | ✅ 可用 | ❌ 不可用 |

---

## ✅ 當前設定

```bash
# 查看當前模式
cd /home/david/tvm/build
grep TVM_MY_DEVICE_USE_LLVM config.cmake
```

目前設定為：**LLVM Mode (ON)** - 適合 RVV 交叉編譯

