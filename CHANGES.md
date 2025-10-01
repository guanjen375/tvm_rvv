# 修改摘要

## ✅ 核心修改

### 1. Codegen - LLVM Backend
**檔案：** `src/target/source/codegen_my_device.cc`

```cpp
Buildmy_device → LLVM backend (支援交叉編譯)
```

**轉傳的參數：**
- `-mtriple`: RISC-V triple
- `-mcpu`, `-mattr`, `-mabi`, `-mfloat-abi`
- 自動補 `+v` 給 RISC-V

### 2. Runtime - 簡化
**檔案：**
- `src/runtime/my_device/my_device_module.cc` - 清空（不再需要）
- `src/runtime/my_device/my_device_common.cc` - 移除編譯邏輯

**原因：**
- LLVM backend 已在 x86 上編譯完成
- export_library 用 cross-compiler 生成 .so
- Runtime 載入的是 DSO module，不需要動態編譯

---

## 🎯 完整流程

### X86 編譯（rvv_test.py）
```
ONNX → Relax IR
  ↓
relax.build(target="my_device -mtriple=riscv64-linux-gnu ...")
  ↓
Buildmy_device → LLVM backend → LLVM module (RISC-V)
  ↓
export_library(cross_compiler="riscv64-linux-gnu-g++")
  ↓
交叉編譯成 mobilenet.so (RISC-V binary)
```

### Banana Pi 執行（run.py）
```
load_module("mobilenet.so")  → 載入 DSO module
  ↓
tvm.my_device(0)  → 設定 device
  ↓
VirtualMachine(lib, device)
  ↓
vm["main"](input)  → 直接執行（已編譯好）
```

---

## 📋 Device 配置

- ✅ kDLmy_device = 17
- ✅ `annotate_device_regions.cc` 強制為 device
- ✅ Host/Device 正確分離

---

## 🔧 重新編譯

```bash
cd /home/david/tvm/build
ninja -j$(nproc)

# 測試
cd /home/david/tvm
python3 rvv_test.py mobilenet
```

