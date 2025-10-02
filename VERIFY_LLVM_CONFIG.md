# 驗證 my_device LLVM Codegen 配置

## 📋 當前配置狀態

### ✅ 已確認配置正確

在 `build/config.cmake` 中：

```cmake
# 第 154 行
set(USE_LLVM /home/david/Downloads/clang+llvm-18.1.7-x86_64-linux-gnu-ubuntu-18.04/bin/llvm-config)

# 第 155 行  
set(TVM_MY_DEVICE_USE_LLVM ON)
```

**結論：配置正確！** ✓

---

## 🔍 如何驗證是否真的使用 LLVM

### 方法 1：編譯時檢查 debug 訊息（推薦）

由於有 libstdc++ 版本衝突問題，需要使用系統 Python：

```bash
# 使用系統 Python（避免 conda libstdc++ 衝突）
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib
/usr/bin/python3 rvv_test.py mobilenet 2>&1 | tee compile_output.txt

# 檢查輸出
grep "my_device codegen" compile_output.txt
```

**預期看到（LLVM 模式）**：
```
[INFO] [my_device codegen] Using LLVM backend mode
```

**如果看到（C++ 模式，錯誤）**：
```
[INFO] [my_device codegen] Using C++ source codegen + runtime compilation mode
```

### 方法 2：檢查編譯參數

```bash
cd build
make VERBOSE=1 | grep TVM_MY_DEVICE_USE_LLVM
```

應該看到：
```
-DTVM_MY_DEVICE_USE_LLVM=1
```

### 方法 3：檢查源碼編譯

查看 `src/target/source/codegen_my_device.cc` 的編譯：

```bash
cd build
make VERBOSE=1 2>&1 | grep "codegen_my_device.cc" | grep -o "\-D[A-Z_]*LLVM[A-Z_]*=[0-9]*"
```

應該包含：
```
-DTVM_MY_DEVICE_USE_LLVM=1
```

### 方法 4：執行測試腳本（如果 Python 可用）

```bash
# 方案 A：使用系統 Python
/usr/bin/python3 test_llvm_codegen.py

# 方案 B：修復 conda 環境
conda install -c conda-forge libstdcxx-ng
python3 test_llvm_codegen.py
```

---

## ⚠️ Libstdc++ 版本衝突問題

### 問題

```
OSError: libstdc++.so.6: version `GLIBCXX_3.4.30' not found
```

### 原因

- TVM 使用系統 GCC 11+ 編譯（需要 GLIBCXX_3.4.30）
- Conda 環境使用較舊的 libstdc++

### 解決方案

#### 方案 A：使用系統 Python

```bash
# 臨時使用系統 Python
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib
/usr/bin/python3 rvv_test.py mobilenet
```

#### 方案 B：更新 Conda 的 libstdc++

```bash
conda install -c conda-forge libstdcxx-ng
```

#### 方案 C：使用系統 library

```bash
# 永久設置（添加到 ~/.bashrc）
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
```

---

## 📝 已添加的 Debug 訊息

在以下文件中添加了路徑追蹤訊息：

### 1. `src/target/source/codegen_my_device.cc`

**LLVM 模式**（第 419 行）：
```cpp
LOG(INFO) << "[my_device codegen] Using LLVM backend mode";
```

**C++ 模式**（第 478 行）：
```cpp  
LOG(INFO) << "[my_device codegen] Using C++ source codegen + runtime compilation mode";
```

### 2. `src/runtime/my_device/my_device_module.cc`

**載入模組**（第 126 行）：
```cpp
LOG(INFO) << "[my_device runtime] Loading my_device binary module (C++ runtime mode)";
```

**執行時**（第 58 行）：
```cpp
LOG(INFO) << "[my_device runtime] Invoking C++ runtime dynamic compilation for function: " << func_name_;
```

---

## ✅ 驗證清單

### 編譯時配置

- [x] `USE_LLVM` 設置為 LLVM config 路徑
- [x] `TVM_MY_DEVICE_USE_LLVM=ON` 在 build/config.cmake
- [ ] 重新編譯 TVM 後驗證宏定義
- [ ] 編譯模型時看到 "Using LLVM backend mode"

### Runtime 驗證

- [ ] 執行時**不**看到 "Loading my_device binary module (C++ runtime mode)"
- [ ] 執行時**不**看到 "call c++ runtime"
- [ ] 執行時**不**看到 "Invoking C++ runtime dynamic compilation"

---

## 🚀 快速驗證命令

```bash
# 1. 確認配置
cat build/config.cmake | grep -A1 -B1 "LLVM"

# 2. 檢查編譯宏（需要重新編譯）
cd build
rm -rf CMakeFiles CMakeCache.txt
cmake ..
make VERBOSE=1 2>&1 | grep "TVM_MY_DEVICE_USE_LLVM"

# 3. 編譯模型並檢查
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/local/lib
/usr/bin/python3 rvv_test.py mobilenet 2>&1 | grep "my_device"

# 4. 查看完整輸出
cat rvv_test_output.txt
```

---

## 📊 預期輸出對比

### ✅ 正確（LLVM 模式）

**編譯時**：
```
[INFO] [my_device codegen] Using LLVM backend mode
Compile to: mobilenet.so
```

**執行時**（如果執行 run.py）：
```
✓ 推論完成！
  - 輸出形狀：(1, 1000)
```

**不會看到**：
- ❌ `[my_device runtime] Loading my_device binary module`
- ❌ `call c++ runtime`
- ❌ `Cannot find function conv2d_kernel`

### ❌ 錯誤（C++ 模式）

**編譯時**：
```
[INFO] [my_device codegen] Using C++ source codegen + runtime compilation mode
```

**執行時**：
```
[INFO] [my_device runtime] Loading my_device binary module (C++ runtime mode)
call c++ runtime
[INFO] [my_device runtime] Invoking C++ runtime dynamic compilation for function: conv2d_kernel
✗ 推論失敗：Cannot find function conv2d_kernel...
```

---

## 🔧 如果仍然是 C++ 模式

### 步驟 1：清理並重新編譯

```bash
cd build
rm -rf CMakeFiles CMakeCache.txt
cmake ..
make -j$(nproc)
```

### 步驟 2：驗證宏定義

```bash
# 查看編譯時的宏
cd build
make VERBOSE=1 2>&1 | grep codegen_my_device.cc | grep -o "\-D[^ ]*"
```

應該包含：
```
-DTVM_MY_DEVICE_USE_LLVM=1
```

### 步驟 3：檢查 CMakeCache.txt

```bash
grep TVM_MY_DEVICE_USE_LLVM build/CMakeCache.txt
```

應該顯示：
```
TVM_MY_DEVICE_USE_LLVM:BOOL=ON
```

---

## 📚 相關文件

- `build/config.cmake` - 當前配置（已確認正確）
- `src/target/source/codegen_my_device.cc` - Codegen 入口（已添加 debug）
- `src/runtime/my_device/my_device_module.cc` - Runtime 模組（已添加 debug）
- `FIX_LLVM_MODE.md` - 完整修復指南
- `DEBUG_SUMMARY.md` - 問題診斷總結

---

## 總結

**當前狀態**：✅ build/config.cmake 配置正確

**下一步**：
1. 確保 TVM 使用這個配置重新編譯
2. 執行 `rvv_test.py` 並檢查 debug 訊息
3. 應該看到 "[my_device codegen] Using LLVM backend mode"

**如何驗證**：編譯模型時查看輸出，應該顯示 LLVM 模式而非 C++ 模式。

