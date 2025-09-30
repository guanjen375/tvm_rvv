# run.py
# 載入 rvv_test.py 產出的 .so 並在 my_device 上執行

import sys
import tvm
from tvm import relax
import numpy as np

# 解析命令列參數
if len(sys.argv) < 2:
    print("Usage: python run.py <model_name> [input_shape]")
    print("Example: python run.py mobilenet")
    sys.exit(1)

NAME = sys.argv[1]
SO_FILE = NAME + ".so"

# 預設輸入形狀（MobileNet）
INPUT_SHAPE = (1, 3, 224, 224)
if len(sys.argv) >= 3:
    # 允許自訂輸入形狀，例如 "1,3,224,224"
    INPUT_SHAPE = tuple(map(int, sys.argv[2].split(",")))

print(f"[INFO] 載入模組：{SO_FILE}")
print(f"[INFO] 輸入形狀：{INPUT_SHAPE}")

# 1) 載入編譯好的 .so 模組
try:
    lib = tvm.runtime.load_module(SO_FILE)
except Exception as e:
    print(f"[ERROR] 無法載入 {SO_FILE}：{e}")
    print("[HINT] 請確認已執行 rvv_test.py 產生 .so 檔案")
    sys.exit(1)

# 2) 設定裝置：使用 CPU
#    重要：雖然編譯時 target 是 'my_device' (RISC-V + RVV)，
#    但 RVV 是 CPU 的向量指令集擴展（類似 x86 的 AVX、ARM 的 NEON），
#    因此 Runtime 應使用 tvm.cpu() 作為執行上下文。
#    TVM Runtime 會自動調用為 RVV 編譯的機器碼。
device = tvm.cpu(0)
print(f"[INFO] 使用裝置：CPU (device_type={device.device_type}, RVV 指令將在 CPU 上執行)")

# 3) 建立 Relax VM 執行器
#    TVM 0.20：使用 relax.VirtualMachine
try:
    vm = relax.VirtualMachine(lib, device)
except Exception as e:
    print(f"[ERROR] 無法建立 VirtualMachine：{e}")
    print("[HINT] 確認 .so 是否為 Relax 模組且包含 'main' 函式")
    sys.exit(1)

# 4) 準備輸入資料（隨機張量，模擬影像輸入）
print("[INFO] 準備輸入資料...")
input_data = np.random.randn(*INPUT_SHAPE).astype("float32")

# 將 NumPy 陣列轉換為 TVM NDArray
try:
    input_tvm = tvm.nd.array(input_data, device=device)
except Exception as e:
    print(f"[ERROR] 無法建立 TVM NDArray：{e}")
    sys.exit(1)

# 5) 執行推論
print("[INFO] 執行推論...")
try:
    # TVM 0.20 Relax API：vm["main"](input)
    output = vm["main"](input_tvm)
    
    # 若輸出為 tuple，取第一個元素
    if isinstance(output, (tuple, list)):
        output = output[0]
    
    # 轉回 NumPy
    output_np = output.numpy()
    
    print("[SUCCESS] 推論完成！")
    print(f"  - 輸出形狀：{output_np.shape}")
    print(f"  - 輸出範圍：[{output_np.min():.6f}, {output_np.max():.6f}]")
    
    # 若為分類任務，顯示前 5 個預測類別
    if output_np.ndim == 2 and output_np.shape[0] == 1:
        top5_idx = np.argsort(output_np[0])[-5:][::-1]
        print(f"  - Top-5 預測類別：{top5_idx.tolist()}")
        print(f"  - Top-5 分數：{output_np[0][top5_idx].tolist()}")
    
except Exception as e:
    print(f"[ERROR] 推論失敗：{e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

print("\n[DONE] 執行完成")
