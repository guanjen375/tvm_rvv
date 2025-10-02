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

DynamicKernel compile_model_dynamic(const std::string& data_, const std::string& func_name) {
  DynamicKernel dk;
  dk.func_ptr = nullptr;  // 預設失敗

  // 取得 HOME 目錄
  const char* home_env = std::getenv("HOME");
  if (home_env == nullptr) {
    std::cerr << "無法取得 HOME 環境變數。\n";
    return dk;
  }
  std::string home_dir(home_env);

  // 1. 採取多線程
  std::string data_modify = update_codegen_multithread(data_);

  // 2. 產生臨時原始檔案：利用 func_name 保證唯一性
  std::string tmp_source = home_dir + "/" + func_name + ".cc";
  std::ofstream ofs(tmp_source);
  if (!ofs.is_open()) {
    std::cerr << "無法打開檔案 " << tmp_source << " 進行寫入。\n";
    return dk;
  }
  ofs << data_modify;
  ofs.close();
  

  // 3. 編譯成共享庫（使用 LLVM/RVV 支持的編譯器）
  std::string so_file = home_dir + "/" + func_name + ".so";
  
  // 檢查環境變數來決定使用哪個編譯器
  const char* compiler_env = std::getenv("TVM_RVV_COMPILER");
  std::string compiler = (compiler_env != nullptr) ? compiler_env : "g++";
  
  // 如果使用 RISC-V 交叉編譯器，添加 RVV 支持的選項
  std::string compile_cmd = compiler + " -O3 -fPIC -shared -fpermissive -fopenmp ";
  
  // 如果是 riscv64 交叉編譯器，添加 RVV 相關選項
  if (compiler.find("riscv64") != std::string::npos || compiler.find("clang") != std::string::npos) {
    compile_cmd += "-march=rv64gcv -mabi=lp64d ";  // 支持 RVV 的選項
  }
  
  compile_cmd += "-o " + so_file + " " + tmp_source;
  compile_cmd += " -I" + home_dir + "/tvm_rvv/include";
  compile_cmd += " -I" + home_dir + "/tvm_rvv/include/dlpack/include";
  compile_cmd += " -I" + home_dir + "/tvm_rvv/3rdparty/dmlc-core/include -lffi";
  
  std::cout << "RVV compile command:" << compile_cmd << "\n";
  int compile_status = std::system(compile_cmd.c_str());
  if (compile_status != 0) {
    std::cerr << "編譯過程出現錯誤，狀態碼：" << compile_status << "\n";
    return dk;
  }

  // 4. 動態載入共享庫
  void* handle = dlopen(so_file.c_str(), RTLD_LAZY);
  if (!handle) {
    std::cerr << "dlopen 錯誤: " << dlerror() << "\n";
    return dk;
  }
  dlerror();  // 清除舊錯誤

  dk.func_ptr = dlsym(handle, func_name.c_str());
  const char* dlsym_err = dlerror();
  if (dlsym_err) {
    std::cerr << "dlsym 取得 " << func_name << " 時發生錯誤: " << dlsym_err << "\n";
    return dk;
  }

  // 5. 從 data_ 中解析函數簽名
  KernelSignature sig = parse_kernel_signature(data_, func_name);

  // 6. 根據參數型態建立 arg_types 向量
  for (const std::string& ptype : sig.param_types) {
    ffi_type* ft = map_type(ptype);
    if (ft != nullptr) {
      dk.arg_types.push_back(ft);
    } else {
      std::cerr << "錯誤處理參數型別: " << ptype << "\n";
    }
  }

  // 7. 設定回傳型態，目前固定為 void（可依需求擴充）
  ffi_type* ret_type = &ffi_type_void;
  size_t nargs = dk.arg_types.size();
  if (ffi_prep_cif(&dk.cif, FFI_DEFAULT_ABI, nargs, ret_type, dk.arg_types.data()) != FFI_OK) {
    std::cerr << "ffi_prep_cif 建立失敗\n";
    dk.func_ptr = nullptr;
  }
  return dk;
}
