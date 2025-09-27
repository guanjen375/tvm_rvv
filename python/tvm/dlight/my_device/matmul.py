from typing import List, Optional, Union
from tvm import tir
from tvm.target import Target
from ..analysis import normalize_prim_func
from ..base import get_extent
from .base import My_deviceScheduleRule

class Matmul(My_deviceScheduleRule):
    """A schedule rule for my_device Matmul operator."""

        #if not isinstance(func, tir.PrimFunc) or not self.is_target_available(target):
        #    return None

    # 檢查此函式是否屬於 my_device
        #target_attr = func.attrs.get("target", None)
        #if target_attr is None or "my_device" not in str(target_attr):
        #    return None

        #sch = tir.Schedule(func)

        # 【關鍵步驟】更新 TIR 函式屬性，設定 calling_conv 為 2
        #gv = list(sch.mod.functions.keys())[0]  # 使用 sch.mod 屬性而非 sch.mod()
        #updated_func = sch.mod.functions[gv].with_attr("calling_conv", 2)
        #sch.mod.update_func(gv, updated_func)  # 移除多餘括號
        #print("Updated calling_conv for device function", gv)

        #return sch
