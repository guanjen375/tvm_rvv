import tvm
from tvm import relax
from tvm import tir
from tvm.ir.module import IRModule
from tvm.script.parser import ir as I, relax as R
from tvm._ffi.runtime_ctypes import Device
from typing import List
import numpy as np
import os
import json

def export_input_metadata():
    home = os.path.expanduser("~")  # ✅ 自動解析目前使用者的 home 路徑
    metadata_path = os.path.join(home, "input_meta.json")

    metadata = {
        "inputs": [
            {"shape": [2, 3], "dtype": "int32", "device": "cpu"},
            {"shape": [3, 4], "dtype": "int32", "device": "cpu"},
            {"shape": [4, 5], "dtype": "int32", "device": "my_device"}
        ]
    }

    with open(metadata_path, "w") as f:
        json.dump(metadata, f, indent=2)
    print(f"✅ metadata 已儲存到 {metadata_path}")


# ----------------------------------------------------------------
# 创建输入数据
# 生成所有元素皆為 2 的 int32 矩陣
#device = tvm.cuda(0)
#device_str = "cuda"
device = tvm.my_device(0)
device_str = "my_device"

devices = [tvm.cpu(0), device]

np_ipt0 = np.full((2, 3), 2, dtype=np.int32)  # shape (2,3)
np_ipt1 = np.full((3, 4), 2, dtype=np.int32)  # shape (3,4)
np_ipt2 = np.full((4, 5), 2, dtype=np.int32)  # shape (4,5)

ipt0 = tvm.nd.array(np_ipt0, devices[0])  # CPU
ipt1 = tvm.nd.array(np_ipt1, devices[0])  # CPU
ipt2 = tvm.nd.array(np_ipt2, devices[1])  # my_device (GPU)
def compile_mod(
        mod: IRModule, 
        ctx: List[Device] = [
        tvm.cpu(),
        device
    ],) -> relax.VirtualMachine:
    mod = relax.transform.RealizeVDevice()(mod)
    mod = relax.transform.LegalizeOps()(mod)
    if(device_str == "cuda"):
        mod = tvm.tir.transform.DefaultGPUSchedule()(mod)
    target = tvm.target.Target(target=device_str, host="c")
    exe = relax.build(mod, target )
    exe.export_library('/home/david/test.so')
    return relax.VirtualMachine(exe, ctx)

# ----------------------------------------------------------------
# 定义 IRModule，注入全局 vdevice 註記
@I.ir_module
class Example:
    I.module_global_infos({
        "vdevice": [
            I.vdevice({"keys": ["c"], "kind": "c", "tag": ""}, 0, "global"),
            I.vdevice(device_str)
        ]
    })

    @R.function
    def executor(
        x: R.Tensor((2, 3), "int32"),
        y: R.Tensor((3, 4), "int32"),
        z: R.Tensor((4, 5), "int32"),
    ) -> R.Tensor((2, 5), "int32"):
        with R.dataflow():
            # 在 CPU 上計算矩陣乘法: (2,3) @ (3,4) -> (2,4)
            lv0: R.Tensor((2, 4), "int32", "c") = R.matmul(x, y)
            # 將結果從 CPU 移動到 GPU (cuda)
            lv1: R.Tensor((2, 4), "int32", device_str) = R.to_vdevice(lv0, device_str)
            # 在 GPU 上計算矩陣乘法: (2,4) @ (4,5) -> (2,5)
            gv: R.Tensor((2, 5), "int32", device_str) = R.matmul(lv1, z)
            R.output(gv)
        return gv

# ----------------------------------------------------------------
# 编译並執行
vm = compile_mod(Example, devices)
#print(ipt0)
#print(ipt1)
#print(ipt2)
res = vm["executor"](ipt0, ipt1, ipt2)


export_input_metadata()
print("Output matrix:\n", res.numpy())
