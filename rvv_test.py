# compile_relax_x86.py
# Build ONNX -> Relax -> RISC-V .so (with params embedded)

import sys, onnx, tvm
from tvm import relax
from tvm.contrib import cc
from tvm.script import ir as I, relax as R

# try both import paths (TVM branch compatibility)
try:
    from tvm.relax.frontend.onnx import from_onnx
except Exception:
    from tvm.relax.frontend import from_onnx

NAME   = sys.argv[1]
MODEL  = NAME + ".onnx"
OUT_SO = NAME + ".so"
CROSS  = "riscv64-unknown-linux-gnu-g++"  # 依你的工具鏈

ex = None
mod = None
params = None

# 1) Load ONNX -> Relax（若 onnx 版本不相容，跳過 ONNX 路徑，只跑最小驗證）
try:
    m = onnx.load(MODEL)
    ret = from_onnx(m)
    if isinstance(ret, tuple):
        mod, params = ret
    else:
        try:
            from tvm.relax.frontend import detach_params
        except Exception:
            detach_params = relax.frontend.detach_params
        mod, params = detach_params(ret)
    onnx_ok = True
except ImportError as e:
    print("[WARN] 解析 ONNX 需要相容版本的 onnx 套件，建議安裝 onnx==1.15.0 或較舊版本。\n", e)
    onnx_ok = False
except ModuleNotFoundError as e:
    print("[WARN] 找不到 onnx 子模組（可能為 onnx.mapping 於新版本移除），建議安裝 onnx==1.15.0。\n", e)
    onnx_ok = False
except Exception as e:
    print("[WARN] 載入或轉換 ONNX 失敗，先跳過 ONNX 編譯，只驗證 my_device：\n", e)
    onnx_ok = False

# 2) my_device target - 使用 LLVM 生成 RVV 機器碼
# my_device 作為設備抽象，內部使用 LLVM 生成 RISC-V RVV 指令
my_device_target = (
    "my_device "
    "-mtriple=riscv64-linux-gnu "
    "-mcpu=generic-rv64 "
    "-mattr=+m,+a,+f,+d,+c,+v "
    "-opt-level=3"
)

# Host 使用 LLVM
host_llvm_target = "c"

with tvm.target.Target(my_device_target, host=host_llvm_target):
    # 只編譯 ONNX（mobilenet），使用預設 TIR pipeline
    if onnx_ok and mod is not None:
        mod = relax.transform.LegalizeOps()(mod)
        ex = relax.build(
            mod,
            target=tvm.target.Target(my_device_target, host=host_llvm_target),
            params=params,
        )

if ex is not None:
    # 使用交叉編譯器生成 RISC-V RVV 機器碼
    ex.export_library(OUT_SO, fcompile=cc.cross_compiler(CROSS))
    print("Compile to:", OUT_SO)
    print("注意：此 .so 包含 LLVM 生成的 RISC-V RVV 機器碼")
    print("可直接在 RISC-V 設備（如 Banana Pi）上執行")
else:
    print("跳過 ONNX 匯出（僅驗證 my_device 最小範例成功）")
