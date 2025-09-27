# compile_relax_x86.py
# Build ONNX -> Relax -> RISC-V .so (with params embedded)

import sys, onnx, tvm
from tvm import relax
from tvm.contrib import cc

# try both import paths (TVM branch compatibility)
try:
    from tvm.relax.frontend.onnx import from_onnx
except Exception:
    from tvm.relax.frontend import from_onnx

NAME   = sys.argv[1]
MODEL  = NAME + ".onnx"
OUT_SO = NAME + ".so"
CROSS  = "riscv64-unknown-linux-gnu-g++"  # 依你的工具鏈

# 1) Load ONNX -> Relax
m = onnx.load(MODEL)

# 可能的回傳差異：有的版本回傳 (mod, params)，有的只回傳 mod
ret = from_onnx(m)  # 若你的 ONNX 已含固定 shape/dtype，這樣即可；否則可自行補 shape_dict/dtype
if isinstance(ret, tuple):
    mod, params = ret
else:
    # 舊版：先把綁在 mod 裡的參數拆出來
    try:
        from tvm.relax.frontend import detach_params
    except Exception:
        # 有些分支掛在 relax.frontend 底下
        detach_params = relax.frontend.detach_params
    mod, params = detach_params(ret)

# 2) RISC-V target（需要 RVV 就留 +v；若裝置不支援，移除 +v）
target = (
    "llvm -device=riscv_cpu "
    "-mtriple=riscv64-unknown-linux-gnu "
    "-mcpu=generic-rv64 "
    "-mattr=+m,+a,+f,+d,+c,+v "
    "-mabi=lp64d"
)

# 3) Build & export .so（參數會嵌進可執行檔）
with tvm.transform.PassContext(opt_level=3):
    ex = relax.build(mod, target=target, params=params)

ex.export_library(OUT_SO, fcompile=cc.cross_compiler(CROSS))
print("Compile to:", OUT_SO)
