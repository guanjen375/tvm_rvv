/*
 * my_device module - 不再需要
 * 
 * 使用 LLVM backend 時：
 * - Buildmy_device 返回 LLVM module
 * - export_library 編譯成 DSO module (.so)
 * - Runtime 直接載入 DSO，不需要 my_device module
 * 
 * 保留此檔案以避免編譯錯誤，但實際上不會被使用。
 */

#include "my_device_module.h"

#include <dmlc/memory_io.h>
#include <tvm/runtime/module.h>
#include <tvm/runtime/registry.h>

#include <string>
#include <unordered_map>

#include "../meta_data.h"
#include "../source_utils.h"

namespace tvm {
namespace runtime {

// 這些函數不會被調用（因為使用 LLVM backend）
// 保留只是為了編譯通過

Module my_deviceModuleCreate(std::string data, std::string fmt,
                             std::unordered_map<std::string, FunctionInfo> fmap,
                             std::string source) {
  LOG(FATAL) << "my_deviceModuleCreate should not be called when using LLVM backend";
  return Module();
}

Module my_deviceModuleLoadBinary(void* strm) {
  LOG(FATAL) << "my_deviceModuleLoadBinary should not be called when using LLVM backend";
  return Module();
}

}  // namespace runtime
}  // namespace tvm

// Register outside namespace
TVM_REGISTER_GLOBAL("runtime.module.loadbinary_my_device")
    .set_body_typed(tvm::runtime::my_deviceModuleLoadBinary);
