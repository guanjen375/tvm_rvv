# 修復 my_device LLVM 模式問題

## 問題診斷

從 `record_run.txt` 的錯誤訊息：
```
Cannot find function conv2d_kernel in the imported modules or global registry.
```

以及從代碼分析發現：**雖然啟用了 LLVM (USE_LLVM=ON)，但 `my_device` 仍使用 C++ runtime 模式**。

## 根本原因

在 `CMakeLists.txt` 中有兩個獨立的選項：

```cmake
# 第 31 行
tvm_option(TVM_MY_DEVICE_USE_LLVM "my_device backend mode: ON=LLVM(cross-compile), OFF=C++ runtime" OFF)
```

**關鍵問題**：`TVM_MY_DEVICE_USE_LLVM` 預設是 `OFF`！

這導致：
- `codegen_my_device.cc` 走 C++ codegen 路徑（第 476-514 行）
- 生成 my_device module（C++ source code），而不是 LLVM module
- Runtime 嘗試動態編譯 C++ code，但找不到 kernel 函數

## 添加的 Debug 訊息

我已經在以下文件中添加了 debug 訊息：

### 1. `src/target/source/codegen_my_device.cc`
- **LLVM 模式**（第 419 行）：
  ```cpp
  LOG(INFO) << "[my_device codegen] Using LLVM backend mode";
  ```

- **C++ 模式**（第 478 行）：
  ```cpp
  LOG(INFO) << "[my_device codegen] Using C++ source codegen + runtime compilation mode";
  ```

### 2. `src/runtime/my_device/my_device_module.cc`
- **載入模組時**（第 126 行）：
  ```cpp
  LOG(INFO) << "[my_device runtime] Loading my_device binary module (C++ runtime mode)";
  ```

- **創建模組時**（第 119 行）：
  ```cpp
  LOG(INFO) << "[my_device runtime] Creating my_device module (C++ runtime mode) with N functions";
  ```

- **執行函數時**（第 58 行）：
  ```cpp
  LOG(INFO) << "[my_device runtime] Invoking C++ runtime dynamic compilation for function: XXX";
  ```

## 解決方案

### 步驟 1：確定 TVM 編譯目錄

```bash
# 找到 TVM 的 build 目錄
cd /path/to/tvm/build  # 或其他編譯目錄
```

### 步驟 2：修改 CMake 配置

編輯 `build/config.cmake`（如果沒有，從 `cmake/config.cmake` 複製）：

```cmake
# 必須同時啟用這兩個選項
set(USE_LLVM ON)                    # 啟用 LLVM
set(TVM_MY_DEVICE_USE_LLVM ON)     # ⚠️ 關鍵！啟用 my_device LLVM 模式
set(USE_my_device ON)               # 啟用 my_device

# 如果需要 RVV，還要設置 LLVM 目標
# set(USE_LLVM "/path/to/llvm-config")
```

### 步驟 3：重新編譯 TVM

```bash
cd build
cmake ..
make -j$(nproc)
```

**檢查編譯訊息**：應該看到 `TVM_MY_DEVICE_USE_LLVM=1` 被定義。

### 步驟 4：驗證配置

```bash
# 檢查 CMakeCache.txt
grep TVM_MY_DEVICE_USE_LLVM build/CMakeCache.txt
# 應該顯示：TVM_MY_DEVICE_USE_LLVM:BOOL=ON
```

或執行診斷腳本：
```bash
python3 diagnose_llvm_mode.py
```

### 步驟 5：重新編譯模型

```bash
python3 compile_model.py mobilenet
```

**預期訊息**：
```
[INFO] [my_device codegen] Using LLVM backend mode
```

**錯誤訊息**（表示仍在 C++ 模式）：
```
[INFO] [my_device codegen] Using C++ source codegen + runtime compilation mode
```

### 步驟 6：執行推論

```bash
python3 run.py mobilenet > output.txt 2>&1
cat output.txt
```

**成功的話不會看到**：
- `[my_device runtime] Loading my_device binary module (C++ runtime mode)`
- `call c++ runtime`

## 模式對比

| 項目 | C++ Runtime 模式 | LLVM 模式 |
|------|-----------------|-----------|
| CMake 選項 | `TVM_MY_DEVICE_USE_LLVM=OFF` | `TVM_MY_DEVICE_USE_LLVM=ON` |
| Codegen | C++ source code | LLVM IR → 機器碼 |
| Module 類型 | my_device module | DSO/LLVM module |
| Runtime | 動態編譯 C++ | 直接載入機器碼 |
| 適用場景 | Host 開發/測試 | 交叉編譯/RVV |
| 錯誤訊息 | "Cannot find function XXX_kernel" | 應該正常執行 |

## 檢查清單

- [ ] `USE_LLVM=ON` ✓
- [ ] `TVM_MY_DEVICE_USE_LLVM=ON` ⚠️ **這是關鍵！**
- [ ] 重新編譯 TVM
- [ ] 看到 "Using LLVM backend mode" 訊息
- [ ] 重新編譯模型
- [ ] .so 文件包含 LLVM 生成的機器碼
- [ ] 執行時不會嘗試載入 my_device module

## 快速驗證命令

```bash
# 1. 檢查當前配置
grep -E "(USE_LLVM|TVM_MY_DEVICE_USE_LLVM)" build/CMakeCache.txt

# 2. 檢查編譯後的宏定義
python3 -c "
import tvm
print('TVM version:', tvm.__version__)
print('TVM path:', tvm.__file__)
"

# 3. 重新編譯並檢查訊息
python3 compile_model.py mobilenet 2>&1 | grep "my_device codegen"

# 4. 執行推論並檢查
python3 run.py mobilenet 2>&1 | grep "my_device"
```

## 如果問題仍然存在

1. **確認使用的 TVM**：
   ```bash
   python3 -c "import tvm; print(tvm.__file__)"
   ```
   確保使用的是你編譯的版本，而不是系統安裝的版本。

2. **清理重編**：
   ```bash
   cd build
   rm -rf *
   cmake .. -DUSE_LLVM=ON -DTVM_MY_DEVICE_USE_LLVM=ON
   make -j$(nproc)
   ```

3. **檢查 compile_model.py**：
   確認編譯時使用的 target 配置正確設置了 LLVM 相關參數（mtriple, mcpu, mattr 等）。

4. **查看完整 log**：
   ```bash
   python3 run.py mobilenet > run_output.txt 2>&1
   cat run_output.txt | grep -A5 -B5 "my_device"
   ```

## 相關文件

- `src/target/source/codegen_my_device.cc`：Codegen 入口，選擇 LLVM 或 C++ 模式
- `src/runtime/my_device/my_device_module.cc`：Runtime 模組載入
- `CMakeLists.txt`：CMake 配置
- `diagnose_llvm_mode.py`：診斷腳本（本次新增）

## 總結

**問題核心**：`TVM_MY_DEVICE_USE_LLVM` 預設 OFF，導致使用 C++ runtime 而非 LLVM。

**解決方法**：設置 `TVM_MY_DEVICE_USE_LLVM=ON` 並重新編譯。

**驗證方法**：編譯模型時應看到 "Using LLVM backend mode" 訊息。

