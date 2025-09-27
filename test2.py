import tvm
from tvm import relax
from tvm.ir.module import IRModule
from tvm.script.parser import ir as I, relax as R
from tvm._ffi.runtime_ctypes import Device
from typing import List
import numpy as np

# ----------------------------------------------------------------
# 建立輸入與參數
device     = tvm.my_device(0)
device_str = "my_device"
devices    = [tvm.cpu(0), device]

# 輸入張量：batch=1, in_channels=3, H=W=5
np_x  = np.random.rand(1, 3, 5, 5).astype("float32")
# 第一層卷積權重＆bias：out=4, in=3, kernel=3x3
np_w0 = np.random.rand(4, 3, 3, 3).astype("float32")
np_b0 = np.random.rand(4).astype("float32")
# 第二層卷積權重＆bias：out=2, in=4, kernel=3x3
np_w1 = np.random.rand(2, 4, 3, 3).astype("float32")
np_b1 = np.random.rand(2).astype("float32")

# 在對應裝置上建立 NDArrays
ipt_x  = tvm.nd.array(np_x,  devices[0])  # CPU
ipt_w0 = tvm.nd.array(np_w0, devices[0])  # CPU
ipt_b0 = tvm.nd.array(np_b0, devices[0])  # CPU
ipt_w1 = tvm.nd.array(np_w1, devices[1])  # my_device
ipt_b1 = tvm.nd.array(np_b1, devices[1])  # my_device

def compile_mod(
    mod: IRModule,
    ctx: List[Device] = devices,
) -> relax.VirtualMachine:
    mod = relax.transform.RealizeVDevice()(mod)
    mod = relax.transform.LegalizeOps()(mod)
    target = tvm.target.Target(device_str, host="c")
    exe = relax.build(mod, target)
    return relax.VirtualMachine(exe, ctx)

# ----------------------------------------------------------------
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
        x:  R.Tensor((1, 3, 5, 5), "float32"),
        w0: R.Tensor((4, 3, 3, 3), "float32"),
        b0: R.Tensor((4,),          "float32"),
        w1: R.Tensor((2, 4, 3, 3), "float32", device_str),
        b1: R.Tensor((2,),         "float32", device_str),
    ) -> R.Tensor((1, 2, 5, 5), "float32"):
        with R.dataflow():
            # 第一層 conv2d（CPU）
            conv0: R.Tensor((1, 4, 5, 5), "float32", "c") = R.nn.conv2d(
                x, w0, strides=[1,1], padding=[1,1,1,1]
            )
            # reshape bias0 -> (1,4,1,1) 並廣播相加
            b0r: R.Tensor((1, 4, 1, 1), "float32", "c") = R.reshape(b0, (1, 4, 1, 1))
            lv0: R.Tensor((1, 4, 5, 5), "float32", "c") = R.add(conv0, b0r)

            # 搬到 my_device
            lv1: R.Tensor((1, 4, 5, 5), "float32", device_str) = R.to_vdevice(
                lv0, device_str
            )

            # 第二層 conv2d（my_device）
            conv1: R.Tensor((1, 2, 5, 5), "float32", device_str) = R.nn.conv2d(
                lv1, w1, strides=[1,1], padding=[1,1,1,1]
            )
            # reshape bias1 -> (1,2,1,1) 並廣播相加
            b1r: R.Tensor((1, 2, 1, 1), "float32", device_str) = R.reshape(b1, (1, 2, 1, 1))
            gv: R.Tensor((1, 2, 5, 5), "float32", device_str) = R.add(conv1, b1r)

            R.output(gv)
        return gv

# ----------------------------------------------------------------
# 編譯並執行
vm  = compile_mod(Example, devices)
out = vm["executor"](ipt_x, ipt_w0, ipt_b0, ipt_w1, ipt_b1)
print("輸出張量形狀：", out.shape)
print(out.numpy())
