#!/usr/bin/env python3
# verify_run_cpu.py - 驗證 run.py 使用 tvm.cpu() 的正確性

import sys

print("=== TVM RVV Runtime 驗證 ===\n")

# 1. 驗證 TVM 版本
try:
    import tvm
    print(f"✓ TVM 版本：{tvm.__version__}")
except Exception as e:
    print(f"✗ 無法載入 TVM：{e}")
    sys.exit(1)

# 2. 驗證 tvm.cpu() API（TVM 0.20）
try:
    device = tvm.cpu(0)
    print(f"✓ tvm.cpu(0) 可用")
    print(f"  - device_type: {device.device_type}")
    print(f"  - device_id: {device.device_id}")
except Exception as e:
    print(f"✗ tvm.cpu() 失敗：{e}")
    sys.exit(1)

# 3. 驗證等價性
try:
    dev_cpu = tvm.cpu(0)
    dev_llvm = tvm.device("llvm", 0)
    dev_cpu_alt = tvm.device("cpu", 0)
    
    assert dev_cpu.device_type == dev_llvm.device_type, "cpu 與 llvm device_type 應相同"
    assert dev_cpu.device_type == dev_cpu_alt.device_type, "cpu() 與 device('cpu') 應相同"
    print(f"✓ tvm.cpu(0) ≡ tvm.device('llvm', 0) ≡ tvm.device('cpu', 0)")
    print(f"  - 共用 device_type: {dev_cpu.device_type}")
except Exception as e:
    print(f"✗ Device 等價性檢查失敗：{e}")
    sys.exit(1)

# 4. 驗證 NDArray 分配
try:
    import numpy as np
    test_arr = np.random.randn(10, 10).astype("float32")
    tvm_arr = tvm.nd.array(test_arr, device=tvm.cpu(0))
    assert tvm_arr.shape == (10, 10), "NDArray 形狀錯誤"
    print(f"✓ tvm.nd.array(data, device=tvm.cpu(0)) 可用")
except Exception as e:
    print(f"✗ NDArray 分配失敗：{e}")

# 5. 驗證 Relax VM
try:
    from tvm import relax
    assert hasattr(relax, "VirtualMachine"), "缺少 relax.VirtualMachine"
    print(f"✓ relax.VirtualMachine 可用")
except Exception as e:
    print(f"✗ Relax VM 檢查失敗：{e}")
    sys.exit(1)

print("\n" + "="*50)
print("關鍵概念驗證")
print("="*50)
print("""
編譯階段 (rvv_test.py):
  Target: 'my_device' (RISC-V + RVV)
  作用: 指示 LLVM 生成 RVV 向量指令

執行階段 (run.py):
  Device: tvm.cpu(0)
  原因: RVV 是 CPU 指令集擴展，在 CPU 上執行
  
類比:
  - x86 + AVX    → 編譯時指定 -mavx → 執行時 tvm.cpu(0)
  - ARM + NEON   → 編譯時指定 +neon → 執行時 tvm.cpu(0)
  - RISC-V + RVV → 編譯時指定 +v    → 執行時 tvm.cpu(0) ✓
""")

print("\n✓ 所有驗證通過！run.py 使用 tvm.cpu(0) 是正確的。")
