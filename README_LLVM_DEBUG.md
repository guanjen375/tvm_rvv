# my_device LLVM Backend Debug 說明

## 📌 快速參考

### ✅ 驗證狀態
**已確認使用 LLVM backend** - 參見 `LLVM_VERIFIED.md`

### 🔧 環境設置
```bash
source env.sh
```

### 📦 編譯模型
```bash
python3 rvv_test.py mobilenet
```

**預期輸出**：
```
[my_device codegen] Using LLVM backend mode  # 多次出現
```

---

## 📝 添加的 Debug 訊息

為了追蹤和驗證 my_device 的執行路徑，在源碼中添加了以下 debug 訊息：

### 1. Codegen 階段 (`src/target/source/codegen_my_device.cc`)

**LLVM 模式（第 419 行）**：
```cpp
LOG(INFO) << "[my_device codegen] Using LLVM backend mode";
```
→ 表示正確使用 LLVM backend

**C++ 模式（第 478 行）**：
```cpp
LOG(INFO) << "[my_device codegen] Using C++ source codegen + runtime compilation mode";
```
→ 表示使用了 C++ runtime（錯誤配置）

### 2. Runtime 階段 (`src/runtime/my_device/my_device_module.cc`)

**載入模組（第 126 行）**：
```cpp
LOG(INFO) << "[my_device runtime] Loading my_device binary module (C++ runtime mode)";
```

**執行函數（第 58 行）**：
```cpp
LOG(INFO) << "[my_device runtime] Invoking C++ runtime dynamic compilation for function: " << func_name_;
```

**注意**：在 LLVM 模式下，這些 runtime 訊息**不應該出現**。

---

## 🎯 判斷標準

### ✅ 正確（LLVM 模式）
編譯時看到：
- ✓ `[my_device codegen] Using LLVM backend mode`（多次）
- ✓ 生成 .o 文件（lib0.o, lib1.o, ...）
- ✓ 使用交叉編譯器連結（riscv64-linux-gnu-g++）

執行時**不會看到**：
- ❌ `[my_device runtime] Loading my_device binary module`
- ❌ `call c++ runtime`

### ❌ 錯誤（C++ 模式）
編譯時看到：
- ❌ `[my_device codegen] Using C++ source codegen + runtime compilation mode`

執行時會看到：
- ❌ `[my_device runtime] Loading my_device binary module (C++ runtime mode)`
- ❌ `[my_device runtime] Invoking C++ runtime dynamic compilation`
- ❌ 錯誤：`Cannot find function XXX_kernel`

---

## 🔍 為什麼需要這些 Debug 訊息

### 問題背景
- TVM 有兩個獨立的配置：`USE_LLVM` 和 `TVM_MY_DEVICE_USE_LLVM`
- 即使啟用了 LLVM，`TVM_MY_DEVICE_USE_LLVM` 預設仍是 OFF
- 導致 my_device 使用 C++ runtime 而非 LLVM

### Debug 訊息的作用
1. **即時驗證**：編譯時立即知道使用哪種模式
2. **問題診斷**：快速定位配置錯誤
3. **追蹤執行路徑**：確認 runtime 行為
4. **未來除錯**：保留以便後續開發

---

## 📚 相關文檔

| 文檔 | 用途 |
|------|------|
| `LLVM_VERIFIED.md` | ✅ 驗證完成報告（**主要參考**） |
| `FIX_LLVM_MODE.md` | 完整修復指南 |
| `DEBUG_SUMMARY.md` | 問題診斷總結 |
| `VERIFY_LLVM_CONFIG.md` | 驗證方法詳解 |
| `BUILD_STATUS.txt` | 配置檢查報告 |
| `CLEANUP_SUMMARY.txt` | 清理記錄 |

---

## ⚙️ 配置要求

### build/config.cmake
```cmake
set(USE_LLVM /path/to/llvm-config)
set(TVM_MY_DEVICE_USE_LLVM ON)      # ⚠️ 關鍵！
set(USE_my_device ON)
```

### 確認方法
```bash
grep TVM_MY_DEVICE_USE_LLVM build/config.cmake
# 應該顯示：set(TVM_MY_DEVICE_USE_LLVM ON)
```

---

## 🚀 工作流程

```
ONNX Model
    ↓
Relax IR
    ↓
my_device Target (配置 RISC-V + RVV)
    ↓
[DEBUG] "[my_device codegen] Using LLVM backend mode"
    ↓
LLVM Codegen (生成 LLVM IR)
    ↓
LLVM Backend (RISC-V code generator)
    ↓
目標文件 (.o files)
    ↓
交叉編譯器連結
    ↓
.so 文件（RISC-V ELF）
```

---

## 💡 使用建議

### 1. 每次修改配置後
```bash
# 重新編譯 TVM
cd build
rm -rf CMakeFiles CMakeCache.txt
cmake ..
make -j$(nproc)

# 驗證配置
grep TVM_MY_DEVICE_USE_LLVM CMakeCache.txt
```

### 2. 編譯模型時
```bash
# 觀察 debug 訊息
python3 rvv_test.py mobilenet 2>&1 | grep "my_device"
```

### 3. 如果懷疑配置不對
```bash
# 檢查是否使用了 LLVM 模式
python3 rvv_test.py mobilenet 2>&1 | head -20
```

---

## 總結

- ✅ Debug 訊息已添加到源碼
- ✅ 可以即時驗證編譯模式
- ✅ 保留這些訊息對未來開發有幫助
- ✅ 當前配置已確認正確（LLVM 模式）

**主要參考文檔**：`LLVM_VERIFIED.md`

