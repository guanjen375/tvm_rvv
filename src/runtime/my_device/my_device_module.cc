/*
 * my_device runtime module
 * 
 * 兩種模式：
 * - TVM_MY_DEVICE_USE_LLVM: 不需要此 module（使用 LLVM/DSO module）
 * - 否則: C++ runtime 動態編譯（需要此 module）
 */

#include "my_device_module.h"

#include <dmlc/memory_io.h>
#include <tvm/runtime/module.h>
#include <tvm/runtime/registry.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "../meta_data.h"
#include "../source_utils.h"

#ifndef TVM_MY_DEVICE_USE_LLVM
#include "my_device_common.h"
#endif

namespace tvm {
namespace runtime {

#ifdef TVM_MY_DEVICE_USE_LLVM
// ========== LLVM 模式：不使用 my_device module ==========
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

#else
// ========== C++ Runtime 動態編譯模式 ==========
struct my_deviceWrappedFunc {
 public:
  void Init(const std::string& func_name, const std::size_t arg_size, const std::string data) {
    func_name_ = func_name;
    arg_size_ = arg_size;
    data_ = data;
  }

  void operator()(TVMArgs args, TVMRetValue* ret, void** addr) const {
    // 準備參數
    std::cout << "call c++ runtime\n";
    std::vector<void*> arg_data(arg_size_, nullptr);
    for (std::size_t i = 0; i < arg_size_; i++) {
      DLTensor* tensor = reinterpret_cast<DLTensor*>(addr[i]);
      arg_data[i] = tensor->data;
    }

    std::vector<void*> arg_ptrs(arg_size_, nullptr);
    for (std::size_t i = 0; i < arg_size_; i++) {
      arg_ptrs[i] = &arg_data[i];
    }

    // 動態編譯並執行
    DynamicKernel dk = compile_model_dynamic(data_, func_name_);
    if (dk.func_ptr == nullptr) {
      LOG(FATAL) << "Failed to compile or load kernel function " << func_name_;
      return;
    }

    dk.call(arg_ptrs.data());
  }

 private:
  std::string data_;
  std::size_t arg_size_;
  std::string func_name_;
};

PackedFunc my_deviceModuleNode::GetFunction(const String& name,
                                            const ObjectPtr<Object>& sptr_to_self) {
  auto it = fmap_.find(name);
  if (it == fmap_.end()) return PackedFunc();
  const FunctionInfo& info = it->second;
  my_deviceWrappedFunc f;
  f.Init(name, info.arg_types.size(), data_);
  return PackFuncVoidAddr(f, info.arg_types);
}

void my_deviceModuleNode::SaveToFile(const String& file_name, const String& format) {
  std::string fmt = GetFileFormat(file_name, format);
  ICHECK_EQ(fmt, fmt_) << "Can only save to format=" << fmt_;
  std::string meta_file = GetMetaFilePath(file_name);
  SaveMetaDataToFile(meta_file, fmap_);
  SaveBinaryToFile(file_name, data_);
}

void my_deviceModuleNode::SaveToBinary(dmlc::Stream* stream) {
  stream->Write(fmt_);
  stream->Write(fmap_);
  stream->Write(data_);
}

String my_deviceModuleNode::GetSource(const String& format) {
  if (format == fmt_) return data_;
  return source_;
}

Module my_deviceModuleCreate(std::string data, std::string fmt,
                             std::unordered_map<std::string, FunctionInfo> fmap,
                             std::string source) {
  auto n = make_object<my_deviceModuleNode>(data, fmt, fmap, source);
  return Module(n);
}

Module my_deviceModuleLoadBinary(void* strm) {
  dmlc::Stream* stream = static_cast<dmlc::Stream*>(strm);
  std::string data;
  std::unordered_map<std::string, FunctionInfo> fmap;
  std::string fmt;
  stream->Read(&fmt);
  stream->Read(&fmap);
  stream->Read(&data);
  return my_deviceModuleCreate(data, fmt, fmap, std::string());
}
#endif

}  // namespace runtime
}  // namespace tvm

// Register outside namespace
TVM_REGISTER_GLOBAL("runtime.module.loadbinary_my_device")
    .set_body_typed(tvm::runtime::my_deviceModuleLoadBinary);
