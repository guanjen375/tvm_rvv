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
 * \file fix_zero_allocations.cc
 * \brief Fix zero-size allocations that can't be handled by LLVM
 */

#include <tvm/runtime/registry.h>
#include <tvm/tir/expr.h>
#include <tvm/tir/stmt_functor.h>
#include <tvm/tir/transform.h>
#include <tvm/arith/analyzer.h>

namespace tvm {
namespace tir {

class ZeroAllocationFixer : public StmtExprMutator {
 public:
  explicit ZeroAllocationFixer(arith::Analyzer* analyzer) : analyzer_(analyzer) {}

  Stmt VisitStmt_(const AllocateNode* op) override {
    // Check if this allocation has a constant size
    int32_t constant_size = op->ConstantAllocationSize();
    
    if (constant_size == 0) {
      // If allocation size is 0, replace with a minimal allocation of size 1
      // This prevents LLVM codegen errors while maintaining correctness
      // (zero-size allocations are typically not accessed anyway)
      Array<PrimExpr> new_extents = {IntImm(DataType::Int(32), 1)};
      
      auto new_alloc = Allocate(op->buffer_var, op->dtype, new_extents,
                                op->condition, this->VisitStmt(op->body),
                                op->annotations, op->span);
      return new_alloc;
    }
    
    // For non-zero allocations, proceed normally
    return StmtExprMutator::VisitStmt_(op);
  }

 private:
  arith::Analyzer* analyzer_;
};

namespace transform {

Pass FixZeroAllocations() {
  auto pass_func = [](PrimFunc f, IRModule m, PassContext ctx) {
    arith::Analyzer analyzer;
    ZeroAllocationFixer fixer(&analyzer);
    auto* n = f.CopyOnWrite();
    n->body = fixer(f->body);
    return f;
  };
  
  return CreatePrimFuncPass(pass_func, 0, "tir.FixZeroAllocations", {});
}

TVM_REGISTER_GLOBAL("tir.transform.FixZeroAllocations")
    .set_body_typed(FixZeroAllocations);

}  // namespace transform
}  // namespace tir
}  // namespace tvm
