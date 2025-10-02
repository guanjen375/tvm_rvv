#include "my_device_module.h"

#include <dmlc/memory_io.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "../source_utils.h"
#include "my_device_common.h"

namespace tvm
{
  namespace runtime
  {
    // 最終版本的 my_deviceWrappedFunc，保持 get_function 輸入不變
    struct my_deviceWrappedFunc
    {
    public:
      void Init(const std::string &func_name, const std::size_t arg_size, const std::string data)
      {
        func_name_ = func_name;
        arg_size_ = arg_size;
        data_ = data;
      }

      void operator()(TVMArgs args, TVMRetValue *ret, void **addr) const
      {
        std::cout << "Data:" << data_;
        // std::cout << "Function name:" << func_name_ << "\n";
        //  1. 建立暫存陣列，存放每個 tensor 的 data 指標（這就是實際的參數值）
        std::vector<void *> arg_data(arg_size_, nullptr);
        for (std::size_t i = 0; i < arg_size_; i++)
        {
          DLTensor *tensor = reinterpret_cast<DLTensor *>(addr[i]);
          arg_data[i] = tensor->data; // 例如 int32_t* 指標
        }

        // 2. 建立一個新的陣列，存放指向每個引數值的指標（即 &arg_data[i]）
        std::vector<void *> arg_ptrs(arg_size_, nullptr);
        for (std::size_t i = 0; i < arg_size_; i++)
        {
          arg_ptrs[i] = &arg_data[i];
        }

        // 3. 動態編譯並取得 DynamicKernel，傳入 codegen 資料與函數名稱
        DynamicKernel dk = compile_model_dynamic(data_, func_name_);
        if (dk.func_ptr == nullptr)
        {
          std::cerr << "Error: failed to compile or load kernel function " << func_name_ << std::endl;
          return;
        }

        // 4. 利用 dk.call 呼叫動態函數，傳入剛組好的引數陣列。這裡由於直接傳入每個 tensor 的 data
        // 指標，
        //    kernel 函數內若有寫入操作，會直接更新原始的 DLTensor (例如 addr[1])
        dk.call(arg_ptrs.data());
      }

    private:
      std::string data_;
      std::size_t arg_size_;
      std::string func_name_;
    };

    PackedFunc my_deviceModuleNode::GetFunction(const String &name,
                                                const ObjectPtr<Object> &sptr_to_self)
    {
      auto it = fmap_.find(name);
      if (it == fmap_.end())
        return PackedFunc();
      const FunctionInfo &info = it->second;
      my_deviceWrappedFunc f;
      f.Init(name, info.arg_types.size(), data_);
      return PackFuncVoidAddr(f, info.arg_types);
    }

    void my_deviceModuleNode::SaveToFile(const String &file_name, const String &format)
    {
      std::string fmt = GetFileFormat(file_name, format);
      ICHECK_EQ(fmt, fmt_) << "Can only save to format=" << fmt_;
      std::string meta_file = GetMetaFilePath(file_name);
      SaveMetaDataToFile(meta_file, fmap_);
      SaveBinaryToFile(file_name, data_);
    }

    void my_deviceModuleNode::SaveToBinary(dmlc::Stream *stream)
    {
      stream->Write(fmt_);
      stream->Write(fmap_);
      stream->Write(data_);
    }

    String my_deviceModuleNode::GetSource(const String &format)
    {
      if (format == fmt_)
        return data_;
      if (fmt_ == "cl")
      {
        return data_;
      }
      else
      {
        return source_;
      }
    }

    Module my_deviceModuleCreate(std::string data, std::string fmt,
                                 std::unordered_map<std::string, FunctionInfo> fmap,
                                 std::string source)
    {
      auto n = make_object<my_deviceModuleNode>(data, fmt, fmap, source);
      return Module(n);
    }

    Module my_deviceModuleLoadBinary(void *strm)
    {
      dmlc::Stream *stream = static_cast<dmlc::Stream *>(strm);
      std::string data;
      std::unordered_map<std::string, FunctionInfo> fmap;
      std::string fmt;
      stream->Read(&fmt);
      stream->Read(&fmap);
      stream->Read(&data);
      return my_deviceModuleCreate(data, fmt, fmap, std::string());
    }

    TVM_REGISTER_GLOBAL("runtime.module.loadbinary_my_device")
        .set_body_typed(my_deviceModuleLoadBinary);
  } // namespace runtime
} // namespace tvm
