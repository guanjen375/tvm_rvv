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

#include "../../meta_schedule/utils.h"

namespace tvm {
namespace tir {
namespace transform {


IRModule MarkScheduled_my_device(const IRModule& mod) {
  Map<GlobalVar, BaseFunc> result;

  for (const auto& [gv, base_func] : mod->functions) {
    if (const auto* prim_func_node = base_func.as<tir::PrimFuncNode>()) {
      tir::PrimFunc prim_func = GetRef<tir::PrimFunc>(prim_func_node);
      tir::PrimFunc new_prim_func =
          WithAttr(std::move(prim_func), tir::attr::kIsScheduled, Bool(true));
      result.Set(gv, new_prim_func);
    } else {
      result.Set(gv, base_func);
    }
  }

  return IRModule(result,              // functions
                  mod->source_map,     // map
                  mod->attrs,          // attrs
                  mod->global_infos);  // global_infos
}


Pass Defaultmy_deviceSchedule() {
  runtime::TypedPackedFunc<IRModule(IRModule, PassContext)> pass_func =  //
      [=](IRModule m, PassContext pc) {
        tir::Schedule sch = tir::Schedule::Traced(m, /*seed=*/-1, /*debug_mask=*/0,
                                                  tir::ScheduleErrorRenderLevel::kDetail);
        for (const auto& [gv, func] : m->functions) {
          if (func->IsInstance<tir::PrimFuncNode>() && !func->HasNonzeroAttr(attr::kIsScheduled) ) {
            // get the target from context.
            tvm::Target target = tvm::Target::Current();
            // get the target from kTarget attribute
            Optional<tvm::Target> func_target =
                func->attrs.GetAttr<tvm::Target>(tvm::attr::kTarget);
            if (func_target.defined()) {
              target = func_target.value();
            }
            ICHECK(target.defined()) << "The target is missing either in the current context or in "
                                        "the prim_func's attribute.";
            
            
            sch->WorkOn(gv->name_hint);
            Array<tir::BlockRV> blocks = meta_schedule::BlockCollector::Collect(sch);
            for (const tir::BlockRV& block : blocks) {
              auto childs = sch->GetChildBlocks(block);
              if (!childs.empty()) {
                continue;
              }
            }
          }
        }
        return MarkScheduled_my_device(sch->mod());
      };
  return CreateModulePass(/*pass_function=*/pass_func,         //
                          /*opt_level=*/0,                     //
                          /*pass_name=*/"Defaultmy_deviceSchedule",  //
                          /*required=*/{});
}

TVM_REGISTER_GLOBAL("tir.transform.Defaultmy_deviceSchedule").set_body_typed(Defaultmy_deviceSchedule);

}  // namespace transform

}  // namespace tir
}  // namespace tvm
