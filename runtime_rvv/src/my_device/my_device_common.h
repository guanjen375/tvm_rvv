#ifndef TVM_RUNTIME_MY_DEVICE_MY_DEVICE_COMMON_H_
#define TVM_RUNTIME_MY_DEVICE_MY_DEVICE_COMMON_H_

#include <ffi.h>
#include <dlfcn.h>
#include <tvm/runtime/c_runtime_api.h>
#include <tvm/runtime/device_api.h>
#include <tvm/runtime/logging.h>
#include <tvm/runtime/memory/memory_manager.h>
#include <tvm/runtime/ndarray.h>
#include <tvm/runtime/packed_func.h>
#include <tvm/runtime/profiling.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <thread>

#include "../file_utils.h"
#include "../meta_data.h"
#include "../pack_args.h"
#include "../texture.h"
#include "../thread_storage_scope.h"

struct KernelSignature {
  std::string return_type;
  std::vector<std::string> param_types;
};

struct DynamicKernel {
  void* func_ptr;                    // 透過 dlsym 得到的函數位址
  ffi_cif cif;                       // libffi 的呼叫介面結構
  std::vector<ffi_type*> arg_types;  // 每個參數的 ffi_type 指標

  // 動態呼叫，此範例假設 kernel 回傳 void，所以 ret_value 為 nullptr
  void call(void** arg_values) { ffi_call(&cif, FFI_FN(func_ptr), nullptr, arg_values); }
};

ffi_type* map_type(const std::string& type_str);
KernelSignature parse_kernel_signature(const std::string& code, const std::string& func_name);
DynamicKernel compile_model_dynamic(const std::string &data_, const std::string &func_name);

namespace tvm {
namespace runtime {

class my_deviceModuleNode : public ModuleNode {
 public:
  explicit my_deviceModuleNode(std::string data, std::string fmt,
                               std::unordered_map<std::string, FunctionInfo> fmap,
                               std::string source)
      : data_(data), fmt_(fmt), fmap_(fmap), source_(source) {};
  PackedFunc GetFunction(const String& name, const ObjectPtr<Object>& sptr_to_self) final;
  const char* type_key() const final { return "my_device"; }
  int GetPropertyMask() const final {
    return ModulePropertyMask::kBinarySerializable | ModulePropertyMask::kRunnable;
  }
  void SaveToFile(const String& file_name, const String& format) final;
  void SaveToBinary(dmlc::Stream* stream) final;
  String GetSource(const String& format) final;

 private:
  std::string data_;
  std::string fmt_;
  std::unordered_map<std::string, FunctionInfo> fmap_;
  std::string source_;
};
}  // namespace runtime
}  // namespace tvm
#endif
