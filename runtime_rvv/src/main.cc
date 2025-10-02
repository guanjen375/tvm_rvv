#include <tvm/runtime/module.h>
#include <tvm/runtime/packed_func.h>
#include <tvm/runtime/ndarray.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <common.h>

using json = nlohmann::json;

int main()
{
    // 取得 HOME 目錄
    const char *home_env = std::getenv("HOME");
    if (home_env == nullptr)
    {
        std::cerr << "無法取得 HOME 環境變數。\n";
    }
    std::string home_dir(home_env);

    std::string so_path;
    std::cout << "請輸入模型 .so 路徑：";
    std::cin >> so_path;

    // 載入 Module 並取得 VM 可執行模組
    tvm::runtime::Module lib = tvm::runtime::Module::LoadFromFile(so_path);
    tvm::runtime::Module vm_mod = lib.GetFunction("vm_load_executable")();

    // 初始化虛擬機（使用 my_device + CPU，my_device 內部使用 LLVM/RVV 編譯）
    // vm_initialization(device_type, device_id, alloc_type, host_device_type, host_device_id, host_alloc_type)
    vm_mod.GetFunction("vm_initialization")(
        static_cast<int>(kDLmy_device), 0, 2,  // device: my_device (LLVM/RVV backend)
        static_cast<int>(kDLCPU), 0, 2         // host: CPU
    );

    // 載入 JSON metadata
    std::string json_path;
    std::cout << "請輸入模型 Json 路徑：";
    std::cin >> json_path;
    std::ifstream ifs(json_path);
    json meta = json::parse(ifs);

    auto input = create_input(meta);
    auto executor = vm_mod.GetFunction("executor");
    tvm::runtime::TVMRetValue out;
    
    // 打包成 TVMArgs 並呼叫 executor
    std::vector<TVMValue> values(input.size());
    std::vector<int> type_codes(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        values[i].v_handle = const_cast<void *>(static_cast<const void *>(input[i].operator->()));
        type_codes[i] = kTVMNDArrayHandle;
    }

    tvm::runtime::TVMArgs args(values.data(), type_codes.data(), input.size());
    executor.CallPacked(args, &out);

    // 印出輸出結果
    tvm::runtime::NDArray output = out;
    print_node(output);

    return 0;
}

