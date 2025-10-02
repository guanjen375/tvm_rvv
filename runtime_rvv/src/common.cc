#include <tvm/runtime/module.h>
#include <tvm/runtime/packed_func.h>
#include <tvm/runtime/ndarray.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using json = nlohmann::json;

DLDevice GetDeviceByStr(const std::string &name)
{
    if (name == "cpu")
        return DLDevice{kDLCPU, 0};
    if (name == "my_device")
        return DLDevice{kDLmy_device, 0};
    throw std::runtime_error("未知裝置: " + name);
}

DLDataType GetDTypeByStr(const std::string &name)
{
    if (name == "int32")
        return DLDataType{kDLInt, 32, 1};
    if (name == "float32")
        return DLDataType{kDLFloat, 32, 1};
    throw std::runtime_error("未知 dtype: " + name);
}

std::vector<tvm::runtime::NDArray> create_input(json meta)
{
    std::vector<tvm::runtime::NDArray> inputs;
    for (const auto &info : meta["inputs"])
    {
        auto shape = info["shape"].get<std::vector<int64_t>>();
        auto dtype = GetDTypeByStr(info["dtype"]);
        auto device = GetDeviceByStr(info["device"]);
        
        // 計算總元素數量（支援任意維度）
        int64_t total_elements = 1;
        for (auto dim : shape) {
            total_elements *= dim;
        }
        
        auto arr = tvm::runtime::NDArray::Empty(shape, dtype, device);
        
        if (dtype.code == kDLInt && dtype.bits == 32)
        {
            std::fill_n(static_cast<int32_t *>(arr->data), total_elements, 2);
        }
        else if (dtype.code == kDLFloat && dtype.bits == 32)
        {
            std::fill_n(static_cast<float *>(arr->data), total_elements, 0.5f);
        }
        inputs.push_back(arr);
    }
    return inputs;
}

void print_node(const tvm::runtime::NDArray &output)
{
    auto dtype = output.DataType();
    int total = 1;
    for (int i = 0; i < output->ndim; ++i)
        total *= output->shape[i];

    std::cout << "輸出結果（shape: ";
    for (int i = 0; i < output->ndim; ++i)
    {
        std::cout << output->shape[i];
        if (i != output->ndim - 1)
            std::cout << "x";
    }
    std::cout << ", dtype: ";
    if (dtype.code() == kDLInt)
        std::cout << "int";
    else if (dtype.code() == kDLFloat)
        std::cout << "float";
    else
        std::cout << "unknown";
    std::cout << dtype.bits() << "):\n";

    if (dtype.code() == kDLInt && dtype.bits() == 32)
    {
        auto *data = static_cast<int32_t *>(output->data);
        for (int i = 0; i < total; ++i)
        {
            std::cout << data[i] << " ";
            if ((i + 1) % output->shape[output->ndim - 1] == 0)
                std::cout << "\n";
        }
    }
    else if (dtype.code() == kDLFloat && dtype.bits() == 32)
    {
        auto *data = static_cast<float *>(output->data);
        for (int i = 0; i < total; ++i)
        {
            std::cout << data[i] << " ";
            if ((i + 1) % output->shape[output->ndim - 1] == 0)
                std::cout << "\n";
        }
    }
    else
    {
        std::cerr << "print_node: 不支援的 dtype！\n";
    }
}

