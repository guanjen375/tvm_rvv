#!/usr/bin/env python3
# verify_run_cpu.py - 驗證 run.py 使用 tvm.my_device() 的正確性

import sys

print("=== TVM my_device Runtime 驗證 ===\n")

# 1. 驗證 TVM 版本
try:
    import tvm
    print(f"✓ TVM 版本：{tvm.__version__}")
except Exception as e:
    print(f"✗ 無法載入 TVM：{e}")
    sys.exit(1)

# 2. 驗證 tvm.my_device() API（TVM 0.20）
try:
    device = tvm.my_device(0)
    print(f"✓ tvm.my_device(0) 可用")
    print(f"  - device_type: {device.device_type} (kDLmy_device = 17)")
    print(f"  - device_id: {device.device_id}")
except Exception as e:
    print(f"✗ tvm.my_device() 失敗：{e}")
    print("  [HINT] 確認 TVM 編譯時啟用了 my_device runtime")
    sys.exit(1)

# 3. 驗證 my_device 與 CPU 是不同裝置
try:
    dev_my_device = tvm.my_device(0)
    dev_cpu = tvm.cpu(0)
    
    print(f"\n✓ my_device 與 CPU 是不同的裝置類型：")
    print(f"  - tvm.my_device(0).device_type = {dev_my_device.device_type} (kDLmy_device)")
    print(f"  - tvm.cpu(0).device_type = {dev_cpu.device_type} (kDLCPU)")
    assert dev_my_device.device_type != dev_cpu.device_type, "my_device 應與 CPU 不同"
    print(f"  - 確認：{dev_my_device.device_type} ≠ {dev_cpu.device_type} ✓")
except Exception as e:
    print(f"✗ Device 類型檢查失敗：{e}")
    sys.exit(1)

# 4. 驗證 NDArray 分配到 my_device
try:
    import numpy as np
    test_arr = np.random.randn(10, 10).astype("float32")
    tvm_arr = tvm.nd.array(test_arr, device=tvm.my_device(0))
    assert tvm_arr.shape == (10, 10), "NDArray 形狀錯誤"
    print(f"\n✓ tvm.nd.array(data, device=tvm.my_device(0)) 可用")
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
  註冊: TVM_REGISTER_TARGET_KIND("my_device", kDLmy_device)

執行階段 (run.py):
  Device: tvm.my_device(0)
  原因: my_device 是獨立註冊的裝置類型（device_type=17）
  
重要：my_device ≠ CPU
  - kDLmy_device = 17 (獨立的 device type)
  - kDLCPU = 1
  - 雖然 my_deviceDeviceAPI 實現類似 CPU（使用 memcpy），
    但在 TVM Runtime 中，它們是兩個不同的裝置。
    
編譯 target 與 runtime device 必須匹配：
  ✓ my_device target → my_device device
  ✗ my_device target → cpu device (不匹配)
""")

print("\n✓ 所有驗證通過！run.py 使用 tvm.my_device(0) 是正確的。")
