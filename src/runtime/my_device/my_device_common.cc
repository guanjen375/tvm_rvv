#include "my_device_common.h"

#include <cstdlib>  // for std::getenv
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

std::string update_codegen_multithread(std::string code) {
  // 找第一個 "for (" 或 "for(" 的出現位置
  size_t pos = code.find("for (");
  if (pos == std::string::npos) {
    pos = code.find("for(");
  }
  if (pos != std::string::npos) {
    // 找到該行起始位置
    size_t line_start = code.rfind("\n", pos);
    if (line_start == std::string::npos) {
      line_start = 0;
    } else {
      line_start++;  // 移到行首
    }
    // 注入 "#pragma omp parallel for" 字串，確保換行
    code.insert(line_start, "#pragma omp parallel for\n");
  } else {
    std::cerr << "Warning: 未找到合適的 for 迴圈，無法注入 OpenMP 指示詞。\n";
  }
  // ------ 結束修改 ------
  return code;
}

// 解析函數簽名不變
KernelSignature parse_kernel_signature(const std::string& code, const std::string& func_name) {
  KernelSignature signature;

  // 過濾掉原始碼中的預處理指令
  std::istringstream iss(code);
  std::string filtered_code;
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line[0] == '#') continue;
    filtered_code += line + "\n";
  }

  // 利用正則表達式匹配函數簽名：<return_type> <func_name>(<param_list>)
  std::regex func_regex(R"(([\w\s\*\d]+)\s+)" + func_name + R"(\s*\(([^)]*)\))");
  std::smatch match;
  if (std::regex_search(filtered_code, match, func_regex)) {
    signature.return_type = match[1].str();
    signature.return_type.erase(0, signature.return_type.find_first_not_of(" \t"));
    signature.return_type.erase(signature.return_type.find_last_not_of(" \t") + 1);

    std::string params = match[2].str();
    std::stringstream ss(params);
    std::string token;
    while (std::getline(ss, token, ',')) {
      token.erase(0, token.find_first_not_of(" \t"));
      token.erase(token.find_last_not_of(" \t") + 1);
      if (!token.empty()) {
        size_t pos = token.find_last_of(' ');
        std::string type_str = (pos != std::string::npos) ? token.substr(0, pos) : token;
        signature.param_types.push_back(type_str);
      }
    }
  } else {
    std::cerr << "無法解析函數 " << func_name << " 的簽名。\n";
  }
  return signature;
}

ffi_type* map_type(const std::string& type_str) {
  if (type_str.find('*') != std::string::npos) {
    return &ffi_type_pointer;
  }
  if (type_str == "int" || type_str == "int32_t") {
    return &ffi_type_sint32;
  }
  if (type_str == "float") {
    return &ffi_type_float;
  }
  if (type_str == "double") {
    return &ffi_type_double;
  }
  std::cerr << "未支援的型別: " << type_str << "\n";
  return nullptr;
}

#ifdef TVM_MY_DEVICE_USE_LLVM
// ========== LLVM 模式：不需要 runtime 動態編譯 ==========
DynamicKernel compile_model_dynamic(const std::string& data_, const std::string& func_name) {
  DynamicKernel dk;
  dk.func_ptr = nullptr;
  LOG(FATAL) << "compile_model_dynamic should not be called when using LLVM backend. "
             << "Functions are already compiled in the DSO module.";
  return dk;
}

#else
// ========== C++ Runtime 動態編譯模式 ==========
// 全局緩存：避免重複編譯同一個模組
static std::mutex compile_cache_mutex;
static std::unordered_map<size_t, std::pair<void*, std::unordered_map<std::string, void*>>> compile_cache;

DynamicKernel compile_model_dynamic(const std::string& data_, const std::string& func_name) {
  DynamicKernel dk;
  dk.func_ptr = nullptr;

  // 計算 hash 作為緩存 key
  std::hash<std::string> hasher;
  size_t data_hash = hasher(data_);
  void* so_handle = nullptr;
  
  // 檢查緩存
  {
    std::lock_guard<std::mutex> lock(compile_cache_mutex);
    auto cache_it = compile_cache.find(data_hash);
    if (cache_it != compile_cache.end()) {
      so_handle = cache_it->second.first;
      auto& func_map = cache_it->second.second;
      auto func_it = func_map.find(func_name);
      if (func_it != func_map.end()) {
        dk.func_ptr = func_it->second;
      }
    }
  }

  // 如果緩存中沒有，需要編譯
  if (dk.func_ptr == nullptr) {
    const char* home_env = std::getenv("HOME");
    if (home_env == nullptr) {
      LOG(ERROR) << "Cannot get HOME environment variable";
      return dk;
    }
    std::string home_dir(home_env);

    // 生成源檔案
    std::string data_modify = update_codegen_multithread(data_);
    std::string tmp_source = home_dir + "/tvm_module_" + std::to_string(data_hash) + ".cc";
    std::ofstream ofs(tmp_source);
    if (!ofs.is_open()) {
      LOG(ERROR) << "Cannot open file " << tmp_source;
      return dk;
    }
    ofs << data_modify;
    ofs.close();

    // 選擇編譯器
    const char* compiler_env = std::getenv("TVM_MY_DEVICE_COMPILER");
    std::string compiler = compiler_env ? compiler_env : "g++";
    
    // 編譯參數
    std::string arch_flags = "";
    if (compiler.find("riscv") != std::string::npos) {
      arch_flags = " -march=rv64gcv -mabi=lp64d";
    }

    // 編譯成共享庫
    std::string so_file = home_dir + "/tvm_module_" + std::to_string(data_hash) + ".so";
    std::string compile_cmd = compiler + " -O2 -fPIC -shared -fpermissive -fopenmp" + 
                              arch_flags + " -o " + so_file + " " + tmp_source;
    compile_cmd += " -I" + home_dir + "/tvm/include";
    compile_cmd += " -I" + home_dir + "/tvm/3rdparty/dlpack/include";
    compile_cmd += " -I" + home_dir + "/tvm/3rdparty/dmlc-core/include -lffi";
    
    int compile_status = std::system(compile_cmd.c_str());
    if (compile_status != 0) {
      LOG(ERROR) << "Compilation failed with status: " << compile_status;
      return dk;
    }

    // 載入共享庫
    so_handle = dlopen(so_file.c_str(), RTLD_LAZY);
    if (!so_handle) {
      LOG(ERROR) << "dlopen error: " << dlerror();
      return dk;
    }

    // 取得函數指標
    dlerror();
    dk.func_ptr = dlsym(so_handle, func_name.c_str());
    const char* dlsym_err = dlerror();
    if (dlsym_err) {
      LOG(ERROR) << "dlsym error for " << func_name << ": " << dlsym_err;
      return dk;
    }

    // 存入緩存
    {
      std::lock_guard<std::mutex> lock(compile_cache_mutex);
      auto& cache_entry = compile_cache[data_hash];
      cache_entry.first = so_handle;
      cache_entry.second[func_name] = dk.func_ptr;
    }
  }

  // 解析函數簽名並準備 FFI
  KernelSignature sig = parse_kernel_signature(data_, func_name);
  for (const std::string& ptype : sig.param_types) {
    ffi_type* ft = map_type(ptype);
    if (ft != nullptr) {
      dk.arg_types.push_back(ft);
    }
  }

  ffi_type* ret_type = &ffi_type_void;
  size_t nargs = dk.arg_types.size();
  if (ffi_prep_cif(&dk.cif, FFI_DEFAULT_ABI, nargs, ret_type, dk.arg_types.data()) != FFI_OK) {
    LOG(ERROR) << "ffi_prep_cif failed";
    dk.func_ptr = nullptr;
  }
  return dk;
}
#endif
