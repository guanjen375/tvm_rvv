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

/*
 * compile_model_dynamic - 不再需要
 * 
 * 使用 LLVM backend 時，所有編譯都在 x86 上由 LLVM + cross-compiler 完成
 * Runtime 直接載入編譯好的 DSO module，不需要動態編譯
 */

DynamicKernel compile_model_dynamic(const std::string& data_, const std::string& func_name) {
  DynamicKernel dk;
  dk.func_ptr = nullptr;
  LOG(FATAL) << "compile_model_dynamic should not be called when using LLVM backend. "
             << "Functions are already compiled in the DSO module.";
  return dk;
}
