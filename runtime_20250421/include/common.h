#ifndef RUNTIME_COMMON_H_
#define RUNTIME_COMMON_H_

#include <vector>
#include <tvm/runtime/ndarray.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::vector<tvm::runtime::NDArray> create_input(json meta);
void print_node(const tvm::runtime::NDArray &output);

#endif