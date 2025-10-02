# Debug 總結：my_device LLVM 模式問題

## 問題現象

執行 `python3 run.py mobilenet` 時出現：
```
Cannot find function conv2d_kernel in the imported modules or global registry.
```

## 根本原因

**TVM 編譯時沒有啟用 `TVM_MY_DEVICE_USE_LLVM`**，導致：

1. ❌ Codegen 走 C++ source 路徑
2. ❌ 生成 my_device module（包含 C++ source）
3. ❌ Runtime 嘗試動態編譯 C++ code
4. ❌ 找不到 kernel 函數

預期應該：
1. ✓ Codegen 走 LLVM 路徑
2. ✓ 生成 LLVM module（包含機器碼）
3. ✓ Runtime 直接載入機器碼
4. ✓ 直接執行，無需編譯

## 已添加的 Debug 訊息

### 編譯時（codegen）

**文件**：`src/target/source/codegen_my_device.cc`

**LLVM 模式**（第 419 行）：
```cpp
LOG(INFO) << "[my_device codegen] Using LLVM backend mode";
```

**C++ 模式**（第 478 行）：
```cpp
LOG(INFO) << "[my_device codegen] Using C++ source codegen + runtime compilation mode";
```

### Runtime 時

**文件**：`src/runtime/my_device/my_device_module.cc`

**載入模組**（第 126 行）：
```cpp
LOG(INFO) << "[my_device runtime] Loading my_device binary module (C++ runtime mode)";
```

**創建模組**（第 119 行）：
```cpp
LOG(INFO) << "[my_device runtime] Creating my_device module (C++ runtime mode) with " 
          << fmap.size() << " functions";
```

**執行函數**（第 58 行）：
```cpp
LOG(INFO) << "[my_device runtime] Invoking C++ runtime dynamic compilation for function: " 
          << func_name_;
```

## 如何診斷

### 方法 1：編譯模型時檢查

```bash
python3 compile_model.py mobilenet 2>&1 | tee compile_output.txt
grep "my_device codegen" compile_output.txt
```

**預期（LLVM 模式）**：
```
[INFO] [my_device codegen] Using LLVM backend mode
```

**實際（C++ 模式）**：
```
[INFO] [my_device codegen] Using C++ source codegen + runtime compilation mode
```

### 方法 2：執行時檢查

```bash
python3 run.py mobilenet 2>&1 | tee run_output.txt
grep "my_device runtime" run_output.txt
```

**如果看到以下訊息，表示在 C++ 模式**：
```
[INFO] [my_device runtime] Loading my_device binary module (C++ runtime mode)
[INFO] [my_device runtime] Invoking C++ runtime dynamic compilation for function: conv2d_kernel
```

### 方法 3：使用診斷腳本

```bash
# 檢查 CMake 配置
python3 diagnose_llvm_mode.py

# 快速驗證編譯模式
python3 verify_llvm_mode.py
```

## 解決步驟

### 1. 找到編譯配置文件

```bash
# 通常在以下位置之一
build/config.cmake
build/CMakeCache.txt
```

### 2. 確認需要的設置

```cmake
set(USE_LLVM ON)                    # ✓ 啟用 LLVM
set(TVM_MY_DEVICE_USE_LLVM ON)     # ⚠️ 關鍵！必須設置
set(USE_my_device ON)               # ✓ 啟用 my_device
```

### 3. 重新編譯 TVM

```bash
cd build
cmake ..
make -j$(nproc)
```

**編譯時應該看到**：
```
-- Build with LLVM ...
-- TVM_MY_DEVICE_USE_LLVM is ON
```

### 4. 重新編譯模型

```bash
python3 compile_model.py mobilenet
```

**應該看到**：
```
[INFO] [my_device codegen] Using LLVM backend mode
```

### 5. 執行推論

```bash
python3 run.py mobilenet
```

**不應該看到**：
- ❌ `[my_device runtime] Loading my_device binary module (C++ runtime mode)`
- ❌ `call c++ runtime`

## 驗證清單

編譯前：
- [ ] `USE_LLVM=ON` 在 config.cmake 中
- [ ] `TVM_MY_DEVICE_USE_LLVM=ON` 在 config.cmake 中（**最關鍵！**）

編譯後：
- [ ] 看到 `TVM_MY_DEVICE_USE_LLVM=1` 被定義
- [ ] `grep TVM_MY_DEVICE_USE_LLVM build/CMakeCache.txt` 顯示 ON

模型編譯時：
- [ ] 看到 "Using LLVM backend mode" 訊息
- [ ] **沒有**看到 "Using C++ source codegen" 訊息

執行時：
- [ ] 正常執行，無錯誤
- [ ] **沒有**看到 "Loading my_device binary module (C++ runtime mode)" 訊息

## 相關文件

### 源碼文件（已修改，添加 debug 訊息）
- `src/target/source/codegen_my_device.cc`
- `src/runtime/my_device/my_device_module.cc`

### 配置文件
- `CMakeLists.txt`（第 31 行定義 TVM_MY_DEVICE_USE_LLVM）
- `cmake/config.cmake`（模板）
- `build/config.cmake`（實際使用）

### 診斷工具（本次新增）
- `diagnose_llvm_mode.py`：檢查 CMake 配置
- `verify_llvm_mode.py`：快速驗證編譯模式
- `FIX_LLVM_MODE.md`：完整的修復指南

## 快速命令參考

```bash
# 1. 檢查當前配置
grep TVM_MY_DEVICE_USE_LLVM build/CMakeCache.txt

# 2. 重新配置並編譯
cd build
cmake .. -DUSE_LLVM=ON -DTVM_MY_DEVICE_USE_LLVM=ON
make -j$(nproc)

# 3. 重新編譯模型並檢查訊息
python3 compile_model.py mobilenet 2>&1 | grep "my_device codegen"

# 4. 執行推論
python3 run.py mobilenet

# 5. 診斷
python3 diagnose_llvm_mode.py
```

## 預期輸出對比

### ❌ 錯誤（C++ 模式）

編譯時：
```
[INFO] [my_device codegen] Using C++ source codegen + runtime compilation mode
```

執行時：
```
[INFO] [my_device runtime] Loading my_device binary module (C++ runtime mode)
[INFO] [my_device runtime] Creating my_device module (C++ runtime mode) with 10 functions
call c++ runtime
[INFO] [my_device runtime] Invoking C++ runtime dynamic compilation for function: conv2d_kernel
✗ 推論失敗：Cannot find function conv2d_kernel...
```

### ✓ 正確（LLVM 模式）

編譯時：
```
[INFO] [my_device codegen] Using LLVM backend mode
```

執行時：
```
✓ 推論完成！
  - 輸出形狀：(1, 1000)
  - 輸出範圍：[...]
```

## 總結

**問題**：`TVM_MY_DEVICE_USE_LLVM` 預設 OFF
**影響**：使用 C++ runtime 而非 LLVM，導致找不到 kernel
**解決**：設置 `TVM_MY_DEVICE_USE_LLVM=ON` 並重新編譯
**驗證**：編譯時看到 "Using LLVM backend mode" 訊息

