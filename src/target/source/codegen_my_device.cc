/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file codegen_my_device.cc
 */
#include "codegen_my_device.h"
#include "../../runtime/my_device/my_device_module.h"

#include "../../runtime/texture.h"
#include "../../runtime/thread_storage_scope.h"
#include "../build_common.h"
#include "../spirv/spirv_utils.h"
#include <tvm/runtime/module.h>
#include <tvm/target/codegen.h>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tvm {
namespace codegen {

CodeGenmy_device::CodeGenmy_device() { module_name_ = name_supply_->FreshName("__tvm_module_ctx"); }

void CodeGenmy_device::Init(bool output_ssa, bool emit_asserts, bool emit_fwd_func_decl,
                        std::string target_str, const std::unordered_set<std::string>& devices) {
  emit_asserts_ = emit_asserts;
  emit_fwd_func_decl_ = emit_fwd_func_decl;
  declared_globals_.clear();
  decl_stream << "// tvm target: " << target_str << "\n";
  decl_stream << "#define TVM_EXPORTS\n";
  decl_stream << "#include \"tvm/runtime/c_runtime_api.h\"\n";
  decl_stream << "#include \"tvm/runtime/c_backend_api.h\"\n";
  decl_stream << "#include <math.h>\n";
  decl_stream << "#include <iostream>\n";
  decl_stream << "#include <stdbool.h>\n";
  CodeGenmy_device::InitGlobalContext();
  CodeGenC::Init(output_ssa);
}

void CodeGenmy_device::InitGlobalContext() {
 //decl_stream << "void* " << tvm::runtime::symbol::tvm_module_ctx << " = NULL;\n";
}

void CodeGenmy_device::DefineModuleName() { decl_stream << "void* " << module_name_ << " = NULL;\n"; }

void CodeGenmy_device::AddFunction(const GlobalVar& gvar, const PrimFunc& func) {
  return AddFunction(gvar, func, /*emit_fwd_func_decl=*/false);
}

void CodeGenmy_device::AddFunction(const GlobalVar& gvar, const PrimFunc& func,
                               bool emit_fwd_func_decl) {
  auto global_symbol = func->GetAttr<String>(tvm::attr::kGlobalSymbol);
  if (global_symbol) {
    function_names_.push_back(global_symbol.value());
  }

  emit_fwd_func_decl_ = emit_fwd_func_decl;
  CodeGenC::AddFunction(gvar, func);
  if (func->HasNonzeroAttr(tir::attr::kIsEntryFunc)) {
    ICHECK(global_symbol.defined())
        << "CodeGenCHost: The entry func must have the global_symbol attribute, "
        << "but function " << gvar << " only has attributes " << func->attrs;

    function_names_.push_back(runtime::symbol::tvm_module_main);
    stream << "// CodegenC: NOTE: Auto-generated entry function\n";
    PrintFuncPrefix(stream);
    PrintType(func->ret_type, stream);
    stream << " " << tvm::runtime::symbol::tvm_module_main
           << "(void* args, int* arg_type_ids, int num_args, void* out_ret_value, "
           << "int* out_ret_tcode, void* resource_handle) {\n";
    stream << "  return " << global_symbol.value()
           << "(args, arg_type_ids, num_args, out_ret_value, out_ret_tcode, resource_handle);\n";
    stream << "}\n";
  }
}

void CodeGenmy_device::GenerateForwardFunctionDeclarations(String global_symbol,
                                                       const Array<Type>& arg_types,
                                                       const Type& ret_type) {
  if (!emit_fwd_func_decl_) {
    return;
  }
  for (auto& func_already_defined : GetFunctionNames()) {
    if (global_symbol == func_already_defined) {
      return;
    }
  }
  this->PrintFuncPrefix(fwd_decl_stream);
  this->PrintType(ret_type, fwd_decl_stream);
  fwd_decl_stream << " " << global_symbol << "(";
  for (size_t i = 0; i < arg_types.size(); ++i) {
    if (i > 0) {
      fwd_decl_stream << ", ";
    }
    CodeGenSourceBase::PrintType(arg_types[i], fwd_decl_stream);
  }
  fwd_decl_stream << ");\n";
}

void CodeGenmy_device::PrintFuncPrefix(std::ostream& os) {  // NOLINT(*)
  os << "#ifdef __cplusplus\n"
     << "extern \"C\"\n"
     << "#endif\n"
     << "TVM_DLL ";
}

void CodeGenmy_device::PrintType(DataType t, std::ostream& os) {  // NOLINT(*)
  int lanes = t.lanes();
  if (t.is_handle()) {
    ICHECK_EQ(lanes, 1) << "does not support vector types";
    os << "void*";
    return;
  }
  if (t.is_void()) {
    os << "void";
    return;
  }
  if (t == DataType::Bool()) {
    os << "bool";
    return;
  }
  bool fail = false;
  if (t.is_float()) {
    switch (t.bits()) {
      case 16:
        os << "half";
        break;
      case 32:
        os << "float";
        break;
      case 64:
        os << "double";
        break;
      default:
        fail = true;
        break;
    }
    if (!fail && lanes == 1) return;
    if (!fail && (lanes >= 2 && lanes <= 16)) {
      os << lanes;
      return;
    }
  } else if (t.is_uint() || t.is_int()) {
    if (t.is_uint()) {
      os << 'u';
    }
    switch (t.bits()) {
      case 8:
        os << "int8_t";
        break;
      case 16:
        os << "int16_t";
        break;
      case 32:
        os << "int32_t";
        break;
      case 64:
        os << "int64_t";
        break;
      case 1:
        os << "int32_t";
        break;
      default:
        fail = true;
        break;
    }
    if (!fail && lanes == 1) return;
    if (!fail && (lanes >= 2 && lanes <= 16)) {
      os << lanes;
      return;
    }
  }
  LOG(FATAL) << "Cannot convert type " << t << " to C type";
}

void CodeGenmy_device::VisitExpr_(const BroadcastNode* op, std::ostream& os) {  // NOLINT(*)
  std::string v = PrintExpr(op->value);
  int lanes = op->dtype.lanes();
  os << "((";
  PrintType(op->dtype, os);
  os << ")(";
  for (int i = 0; i < lanes; ++i) {
    if (i != 0) os << ", ";
    os << v;
  }
  os << "))";
}

void CodeGenmy_device::PrintGetFuncFromBackend(const std::string& func_name,
                                           const std::string& packed_func_name) {                                 
  this->PrintIndent();
  this->stream << "if (" << packed_func_name << " == NULL) {\n";
  int packed_func_if_scope = this->BeginScope();
  this->PrintIndent();
  this->stream << "if (TVMBackendGetFuncFromEnv(" << module_name_ << ", \"" << func_name << "\""
               << ", &" << packed_func_name << ") != 0) {\n";
  int get_func_env_scope = this->BeginScope();
  this->PrintIndent();
  this->stream << "return -1;\n";
  this->EndScope(get_func_env_scope);
  this->PrintIndent();
  this->stream << "}\n";
  this->EndScope(packed_func_if_scope);
  this->PrintIndent();
  this->stream << "}\n";
}

void CodeGenmy_device::PrintFuncCall(const std::string& packed_func_name, int num_args) {
  this->PrintIndent();
  std::string ret_val = name_supply_->FreshName("ret_val");
  std::string ret_type_code = name_supply_->FreshName("ret_type_code");
  this->stream << "TVMValue " << ret_val << ";\n";
  this->PrintIndent();
  this->stream << "int " << ret_type_code << ";\n";
  this->PrintIndent();
  this->stream << "if (TVMFuncCall(" << packed_func_name << ", "
               << "(TVMValue*) stack_value"
               << ", "
               << "(int*) stack_tcode"
               << ", " << num_args << ", "
               << "&" << ret_val << ", "
               << "&" << ret_type_code << ") != 0) {\n";
  int func_call_scope = this->BeginScope();
  this->PrintIndent();
  this->stream << "return -1;\n";
  this->EndScope(func_call_scope);
  this->PrintIndent();
  this->stream << "}\n";
}

void CodeGenmy_device::PrintFuncCallC(const std::string& packed_func_name, int num_args,
                                  const std::string& resource_handle_name) {
                                    
  this->PrintIndent();
  std::string ret_val = name_supply_->FreshName("ret_val");
  std::string ret_type_code = name_supply_->FreshName("ret_type_code");
  this->stream << "TVMValue " << ret_val << ";\n";
  this->PrintIndent();
  this->stream << "int " << ret_type_code << ";\n";
  this->PrintIndent();

  this->stream << "if (" << packed_func_name << "( "
               << "(TVMValue*) stack_value "
               << ", "
               << "(int*) stack_tcode"
               << ", " << num_args << ", "
               << "&" << ret_val << ", "
               << "&" << ret_type_code << ", " << resource_handle_name << ") != 0){\n";

  int func_call_scope = this->BeginScope();
  this->PrintIndent();
  this->stream << "return -1;\n";
  this->EndScope(func_call_scope);
  this->PrintIndent();
  this->stream << "}\n";
}

std::string CodeGenmy_device::GetPackedName(const CallNode* op) {
  const StringImmNode* s = op->args[0].as<StringImmNode>();
  ICHECK(s != nullptr) << "tvm_call_packed_lowered expects first argument as function name";
  std::string func_name = s->value;
  std::string packed_func_name = func_name + "_packed";
  std::string unique_name;
  auto it = declared_globals_.find(packed_func_name);
  if (it != declared_globals_.end()) {
    unique_name = it->second;
  } else {
    unique_name = name_supply_->FreshName(packed_func_name);
    declared_globals_[packed_func_name] = unique_name;
    decl_stream << "static void* " << unique_name << " = NULL;\n";
  }
  return unique_name;
}

CodeGenmy_device::FunctionInfo CodeGenmy_device::GetFunctionInfo(const CallNode* op,
                                                         bool has_resource_handle) {
  const StringImmNode* s = op->args[0].as<StringImmNode>();
  ICHECK(s != nullptr) << "tvm_call_[c]packed_lowered expects first argument as function name";
  int64_t begin = op->args[3].as<IntImmNode>()->value;
  int64_t end = op->args[4].as<IntImmNode>()->value;
  int64_t num_args = end - begin;
  ICHECK_GE(num_args, 0);
  std::string func_name = s->value;

  if (has_resource_handle) {
    const StringImmNode* resource_handle_var = op->args[5].as<StringImmNode>();
    if (resource_handle_var != nullptr) {
      std::string resource_handle_name = resource_handle_var->value;
      return {func_name, num_args - 1, resource_handle_name};
    } else {
      // The final arg should be "(void*) NULL" to indicate the empty resource_handle.
      num_args--;

      const CallNode* reinterpret_call = op->args[5].as<CallNode>();
      ICHECK_NE(reinterpret_call, (void*)nullptr)
          << "At CallNode to " << s
          << "arg 5: Expect either StringImm naming the resource_handle var from interface API or "
          << "reinterpret(0); got: " << op->args[5];
      ICHECK_EQ(reinterpret_call->op, builtin::reinterpret())
          << "At CallNode to " << s
          << "arg 5: Expect either StringImm naming the resource_handle var from interface API or "
          << "reinterpret(0); got: " << op->args[5];
      ICHECK(is_zero(reinterpret_call->args[0])) << "At CallNode to " << s
                                                 << " arg 5: Expect either StringImm naming the "
                                                    "resource_handle var from interface API, or "
                                                 << "zero; got " << op->args[5];
    }
  }
  return {func_name, num_args, "NULL"};
}

void CodeGenmy_device::VisitExpr_(const CallNode* op, std::ostream& os) {  // NOLINT(*)
  if (op->op.same_as(builtin::tvm_stack_alloca())) {
    std::string stack_name = name_supply_->FreshName("stack");
    const std::string& type = op->args[0].as<StringImmNode>()->value;
    const IntImmNode* num = op->args[1].as<IntImmNode>();
    ICHECK(num != nullptr);
    static_assert(alignof(TVMValue) % alignof(DLTensor) == 0, "invariant");
    size_t unit = sizeof(TVMValue);
    size_t size = 0;
    if (type == "shape") {
      size = (num->value * sizeof(tvm_index_t) + unit - 1) / unit;
    } else if (type == "arg_value") {
      size = (num->value * sizeof(TVMValue) + unit - 1) / unit;
    } else if (type == "arg_tcode") {
      size = (num->value * sizeof(int) + unit - 1) / unit;
    } else if (type == "array") {
      size = (num->value * sizeof(DLTensor) + unit - 1) / unit;
    } else {
      LOG(FATAL) << "Unknown stack alloca type " << type;
    }
    this->PrintIndent();
    this->stream << "TVMValue " << stack_name << "[" << size << "];\n";
    os << stack_name;
  } else if (op->op.same_as(builtin::tvm_call_packed_lowered())) {
    auto function_info = GetFunctionInfo(op, false /* has_resource_handle */);
    std::string func_name_packed = GetPackedName(op);
    this->PrintGetFuncFromBackend(function_info.func_name, func_name_packed);
    this->PrintFuncCall(func_name_packed, function_info.num_args);
  } else if (op->op.same_as(builtin::tvm_call_cpacked_lowered())) {
    auto function_info = GetFunctionInfo(op, true /* has_resource_handle */);
    this->PrintFuncCallC(function_info.func_name, function_info.num_args,
                         function_info.resource_handle_name);
  } else if (op->op.same_as(builtin::tvm_throw_last_error())) {
    this->PrintIndent();
    this->stream << "return -1;\n";
  } else {
    CodeGenC::VisitExpr_(op, os);
  }
}

void CodeGenmy_device::VisitStmt_(const AssertStmtNode* op) {  // NOLINT(*)
  if (emit_asserts_) {
    std::string cond = PrintExpr(op->condition);
    PrintIndent();
    stream << "if (!(" << cond << ")) {\n";
    int assert_if_scope = this->BeginScope();
    PrintIndent();
    stream << "TVMAPISetLastError(\"" << op->message.as<StringImmNode>()->value << "\");\n";
    PrintIndent();
    stream << "return -1;\n";
    this->EndScope(assert_if_scope);
    PrintIndent();
    stream << "}\n";
  }
  this->PrintStmt(op->body);
}

void CodeGenmy_device::VisitExpr_(const MinNode* op, std::ostream& os) {  // NOLINT(*)
  PrintTernaryCondExpr(op, "<", os);
}

void CodeGenmy_device::VisitExpr_(const MaxNode* op, std::ostream& os) {  // NOLINT(*)
  PrintTernaryCondExpr(op, ">", os);
}

template <typename T>
inline void CodeGenmy_device::PrintTernaryCondExpr(const T* op, const char* compare,
                                               std::ostream& os) {  // NOLINT(*)
  std::ostringstream temp_a;
  VisitExpr(op->a, temp_a);
  std::string a_id = SSAGetID(temp_a.str(), op->a.dtype());
  std::ostringstream temp_b;
  VisitExpr(op->b, temp_b);
  std::string b_id = SSAGetID(temp_b.str(), op->b.dtype());

  os << "((" << a_id << ") " << compare << " (" << b_id << ") "
     << "? (" << a_id << ") : (" << b_id << "))";
}

runtime::Module Buildmy_device(IRModule mod, Target target) {
  // 臨時路由：將 my_device 的 device-side 編譯直接交給 LLVM 後端
  using tvm::runtime::Registry;
  // 構造一個 LLVM target，若 my_device 帶有 RVV/LLVM 相關屬性，則轉傳給 LLVM
  std::string llvm_target_str = "llvm";

  if (auto v = target->GetAttr<String>("mtriple")) {
    llvm_target_str += " -mtriple=" + v.value();
  }
  if (auto v = target->GetAttr<String>("mcpu")) {
    llvm_target_str += " -mcpu=" + v.value();
  }
  if (auto v = target->GetAttr<Array<String>>("mattr")) {
    std::string feats;
    bool first = true;
    for (const String& s : v.value()) {
      if (!first) feats += ",";
      feats += s;
      first = false;
    }
    if (!feats.empty()) {
      llvm_target_str += " -mattr=" + feats;
    }
  }
  if (auto v = target->GetAttr<runtime::Int>("vector-width")) {
    llvm_target_str += " -vector-width=" + std::to_string(static_cast<int>(v.value()));
  }
  if (auto v = target->GetAttr<runtime::Int>("opt-level")) {
    llvm_target_str += " -opt-level=" + std::to_string(static_cast<int>(v.value()));
  }
  if (auto v = target->GetAttr<String>("jit")) {
    llvm_target_str += " -jit=" + v.value();
  }

  // 若使用者指定了 riscv mtriple 但未指定 +v，預設補上 RVV 特徵與保守的 vector-width
  if (llvm_target_str.find("-mtriple=") != std::string::npos) {
    auto pos = llvm_target_str.find("-mtriple=");
    auto triple = llvm_target_str.substr(pos + 9);
    if (triple.find("riscv") != std::string::npos) {
      if (llvm_target_str.find("-mattr=") == std::string::npos) {
        llvm_target_str += " -mattr=+v";
      }
      if (llvm_target_str.find("-vector-width=") == std::string::npos) {
        // 若未指定，預設 256 bits；可由使用者覆寫為實際 VLEN
        llvm_target_str += " -vector-width=256";
      }
    }
  }

  Target llvm_tgt = Target(llvm_target_str);
  const PackedFunc* bf = Registry::Get("target.build.llvm");
  ICHECK(bf != nullptr) << "target.build.llvm is not enabled";
  return (*bf)(mod, llvm_tgt);
}

TVM_REGISTER_GLOBAL("target.build.my_device").set_body_typed(Buildmy_device);
}  // namespace codegen
}  // namespace tvm
