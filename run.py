import tvm
from tvm import relax
import numpy as np
import time
import os

def main():
    # --- 1. 配置設定 ---
    LIB_PATH = "./mobilenet.so"
    
    # MobileNet 的標準輸入形狀和數據類型 (請根據您的模型確認)
    INPUT_SHAPE = (1, 3, 224, 224)
    DTYPE = "float32"

    print("--- TVM Relax Python Runtime ---")

    # 檢查 .so 檔案是否存在
    if not os.path.exists(LIB_PATH):
        print(f"Error: Library file not found at {LIB_PATH}")
        print("Please run rvv_test.py first to generate mobilenet.so")
        return

    # --- 2. 定義執行設備 (Device Context) ---
    # 雖然您的 target 是 'my_device' (RVV)，但它是 CPU 的擴展指令集。
    # 因此，在 Runtime 中，我們使用 tvm.cpu() 作為執行上下文。
    # TVM Runtime 會自動調用為 RVV 編譯的代碼。
    device = tvm.cpu(0)
    print(f"Executing on device: {device}")

    # --- 3. 載入編譯後的庫 (.so) ---
    print(f"Loading library from: {LIB_PATH}")
    try:
        # 使用 tvm.runtime.load_module 載入 .so
        lib = tvm.runtime.load_module(LIB_PATH)
    except Exception as e:
        print(f"\nError loading module: {e}")
        return

    # --- 4. 初始化 Relax 虛擬機 (VM) ---
    # VM 負責解釋和執行編譯後的模型。
    vm = relax.VirtualMachine(lib, device)
    print("Relax VM initialized.")

    # --- 5. 準備輸入數據 ---
    # 這裡我們使用隨機數據作為範例。在實際應用中，請替換為圖像加載和預處理邏輯。
    print("Preparing input data...")
    np_data = np.random.uniform(0, 1, size=INPUT_SHAPE).astype(DTYPE)
    
    # 將 NumPy 數組轉換為 TVM NDArray，並將其放置在目標設備上
    input_tensor = tvm.nd.array(np_data, device)

    # Relax 模型通常可以直接接受位置參數。
    model_inputs = input_tensor

    # --- 6. 執行模型 ---
    print("Running inference...")
    
    # 調用 VM 中的 "main" 函數來執行模型。"main" 是 Relax 模型預設的入口函數名稱。
    try:
        start_time = time.time()
        output = vm["main"](model_inputs)
        # 確保所有操作完成 (對於準確計時很重要)
        device.sync() 
        end_time = time.time()
    except Exception as e:
        print(f"\nError during execution: {e}")
        print("如果錯誤提到參數不匹配 (mismatched arguments)，")
        print("請嘗試確認您的輸入變數名稱 (例如 'data') 並改用字典傳入參數：")
        print("例如： output = vm[\"main\"]({\"data\": input_tensor})")
        return
    
    print(f"Inference finished in {(end_time - start_time)*1000:.2f} ms.")

    # --- 7. 處理輸出結果 ---
    # 將輸出從 TVM NDArray 轉換回 NumPy 數組進行分析
    
    # Relax 的輸出可能是結構化的 (ADT/Tuple)。對於 MobileNet，通常是單個 Tensor。
    if isinstance(output, tvm.runtime.container.ADT):
        # 如果是結構化輸出，取第一個元素
        if len(output) == 0:
            print("Error: Output structure is empty.")
            return
        output_np = output[0].numpy()
    else:
        # 如果是單個 Tensor 輸出
        output_np = output.numpy()

    # 計算 Top-1 預測結果
    top1_index = np.argmax(output_np[0])
    top1_score = output_np[0][top1_index]

    print("\n--- Results ---")
    print(f"Output shape: {output_np.shape}")
    print(f"Top-1 Prediction Index: {top1_index}")
    print(f"Top-1 Score: {top1_score}")

    # --- 8. (可選) 使用 Time Evaluator 進行性能評估 ---
    print("\n--- Benchmarking (using time_evaluator) ---")
    # 使用 TVM 內建的 time_evaluator 進行多次運行，獲得更精確的性能數據
    try:
        # 評估 "main" 函數，運行 10 次，重複 3 輪
        ftimer = vm.time_evaluator("main", device, number=10, repeat=3)
        prof_res = np.array(ftimer(model_inputs).results) * 1000  # 轉換為毫秒
        print(f"Mean inference time: {np.mean(prof_res):.2f} ms (std: {np.std(prof_res):.2f} ms)")
    except Exception as e:
        print(f"Benchmarking failed: {e}")


if __name__ == "__main__":
    main()
