# 完整流程说明

## 问题回答

### Q: LLVM module 为什么不走 GetFunction 和 operator?

**A**: 因为我们使用的是 **my_device target**，不是纯 LLVM target。

### 流程对比

#### ❌ 错误理解（纯 LLVM）
```
TVM 编译 → LLVM IR → LLVM 编译器 → 机器码 .so
运行时 → dlopen .so → 直接调用函数（不走 my_device）
```

#### ✅ 正确流程（my_device + JIT RVV）
```
TVM 编译 → my_device C codegen → 嵌入 .so（字符串）
运行时 → 加载 .so → my_device Module → GetFunction 
      → operator() → JIT 编译（RVV）→ 执行
```

## 详细流程

### 阶段 1：编译时（rvv_test.py）

```python
# rvv_test.py
target = "my_device"  # ← 关键：使用 my_device target
host = "llvm"

with tvm.target.Target(target, host=host):
    mod = relax.transform.LegalizeOps()(mod)
    ex = relax.build(mod, target=target, params=params)

# 不使用 fcompile - 关键！
ex.export_library("mobilenet.so")  # 不带 fcompile 参数
```

**生成的 .so 包含**：
- VM bytecode
- Function metadata (fmap_)
- **my_device C codegen**（字符串形式，存储在 data_ 字段）

**重要**：如果使用 `fcompile=cc.cross_compiler(...)`，会把 C codegen 预编译成机器码，运行时就无法提取源码了！

### 阶段 2：加载（runtime_rvv/main.cc）

```cpp
// 加载 .so
Module lib = Module::LoadFromFile("mobilenet.so");
Module vm_mod = lib.GetFunction("vm_load_executable")();

// VM 初始化（使用 my_device）
vm_mod.GetFunction("vm_initialization")(
    static_cast<int>(kDLmy_device), 0, 2,  // ← 主设备：my_device
    static_cast<int>(kDLCPU), 0, 2
);
```

**此时**：
- .so 已加载到内存
- VM 准备好
- my_device Module 已注册

### 阶段 3：执行 kernel（首次调用）

```cpp
// 调用 executor
auto executor = vm_mod.GetFunction("executor");
executor(input_arrays...);
```

**内部流程**：

#### 3.1 VM 调度 → my_device Module

```cpp
// runtime_rvv/src/relax_vm/vm.cc
// VM 查找并调用 kernel
PackedFunc kernel_func = module_->GetFunction(kernel_name);
kernel_func(args...);
```

#### 3.2 my_device Module::GetFunction

```cpp
// runtime_rvv/src/my_device/my_device_module.cc:66-76
PackedFunc my_deviceModuleNode::GetFunction(
    const String &name,
    const ObjectPtr<Object> &sptr_to_self) {
    
    auto it = fmap_.find(name);  // 查找函数 metadata
    if (it == fmap_.end()) return PackedFunc();
    
    const FunctionInfo &info = it->second;
    my_deviceWrappedFunc f;
    f.Init(name, info.arg_types.size(), data_);  // ← data_ 包含 C codegen
    return PackFuncVoidAddr(f, info.arg_types);
}
```

**关键**：`data_` 包含完整的 C++ codegen 字符串！

#### 3.3 my_deviceWrappedFunc::operator()

```cpp
// runtime_rvv/src/my_device/my_device_module.cc:27-58
void operator()(TVMArgs args, TVMRetValue *ret, void **addr) const {
    std::cout << "Data:" << data_;  // ← 显示 C codegen
    
    // 准备参数
    std::vector<void *> arg_data(arg_size_);
    for (size_t i = 0; i < arg_size_; i++) {
        DLTensor *tensor = reinterpret_cast<DLTensor *>(addr[i]);
        arg_data[i] = tensor->data;
    }
    
    // JIT 编译！
    DynamicKernel dk = compile_model_dynamic(data_, func_name_);
    
    // 调用编译后的函数
    dk.call(arg_ptrs.data());
}
```

#### 3.4 JIT 编译器（关键步骤）

```cpp
// runtime_rvv/src/my_device/my_device_common.cc:90-141
DynamicKernel compile_model_dynamic(
    const std::string& data_,      // C codegen 字符串
    const std::string& func_name)  // 函数名
{
    // 1. 写入临时文件
    std::string tmp_source = home_dir + "/" + func_name + ".cc";
    std::ofstream ofs(tmp_source);
    ofs << data_;  // ← 写入 C codegen
    ofs.close();
    
    // 2. 检查环境变量
    const char* compiler_env = std::getenv("TVM_RVV_COMPILER");
    std::string compiler = (compiler_env != nullptr) ? compiler_env : "g++";
    
    // 3. 构建编译命令
    std::string compile_cmd = compiler + " -O3 -fPIC -shared ";
    
    // 4. 如果是 riscv64 编译器，添加 RVV 选项
    if (compiler.find("riscv64") != std::string::npos) {
        compile_cmd += "-march=rv64gcv -mabi=lp64d ";  // ← RVV 支持！
    }
    
    compile_cmd += "-o " + so_file + " " + tmp_source;
    
    std::cout << "RVV compile command:" << compile_cmd << "\n";
    
    // 5. 执行编译
    std::system(compile_cmd.c_str());
    
    // 6. dlopen 加载编译后的 .so
    void* handle = dlopen(so_file.c_str(), RTLD_LAZY);
    
    // 7. dlsym 获取函数指针
    dk.func_ptr = dlsym(handle, func_name.c_str());
    
    // 8. 设置 libffi 调用接口
    ffi_prep_cif(&dk.cif, FFI_DEFAULT_ABI, ...);
    
    return dk;
}
```

#### 3.5 执行 RVV 代码

```cpp
// runtime_rvv/src/my_device/my_device_common.h:43
void DynamicKernel::call(void** arg_values) {
    ffi_call(&cif, FFI_FN(func_ptr), nullptr, arg_values);
    // ↑ 通过 libffi 调用编译好的 RVV 函数
}
```

## 为什么必须这样做？

### 方案 A：预编译（不行）
```python
# rvv_test.py
ex.export_library("model.so", fcompile=cc.cross_compiler("riscv64-g++"))
```

**问题**：
- .so 包含的是 riscv64 机器码
- x86 主机无法直接执行
- 无法切换编译器
- 调试困难

### 方案 B：JIT 编译（✅ 正确）
```python
# rvv_test.py
ex.export_library("model.so")  # 不带 fcompile
```

**优势**：
- .so 包含 C codegen 字符串（可移植）
- 运行时根据环境选择编译器
- 可以在 x86 上用 g++ 测试
- 可以在 RISC-V 上用 riscv64-g++ 执行
- 可以查看和调试生成的 C 代码

## 环境变量作用

### TVM_RVV_COMPILER 不设置（默认）
```bash
./exec.out
```
→ 使用 `g++` → x86 机器码 → 本地测试

### TVM_RVV_COMPILER="riscv64-linux-gnu-g++"
```bash
export TVM_RVV_COMPILER="riscv64-linux-gnu-g++"
./exec.out
```
→ 使用 `riscv64-linux-gnu-g++` → 添加 `-march=rv64gcv` → RISC-V RVV 机器码

## 临时文件

运行后会在 `$HOME` 生成：

```
$HOME/
├── fused_add_kernel.cc           # C codegen 源码
├── fused_add_kernel.so           # 编译后的机器码
├── fused_multiply_kernel.cc
├── fused_multiply_kernel.so
└── ...
```

可以查看这些文件来调试！

## 完整示例

```bash
# 1. 编译 runtime
cd /home/david/tvm_rvv/runtime_rvv
make build

# 2. 生成模型（使用修改后的 rvv_test.py）
cd /home/david/tvm_rvv
python3 rvv_test.py mobilenet
# 输出：mobilenet.so（包含 my_device C codegen）

# 3. 创建 metadata
cat > mobilenet_meta.json << 'EOF'
{
  "inputs": [{
    "name": "input0",
    "shape": [1, 3, 224, 224],
    "dtype": "float32",
    "device": "my_device"
  }]
}
EOF

# 4. 本地测试（g++）
cd runtime_rvv
./exec.out
# 输入：../mobilenet.so
# 输入：../mobilenet_meta.json
# 观察输出：
# - Data: <C codegen>
# - RVV compile command: g++ ...

# 5. RVV 执行（riscv64-g++）
export TVM_RVV_COMPILER="riscv64-linux-gnu-g++"
./exec.out
# 输入：../mobilenet.so
# 输入：../mobilenet_meta.json
# 观察输出：
# - Data: <C codegen>
# - RVV compile command: riscv64-linux-gnu-g++ ... -march=rv64gcv ...

# 6. 检查生成的文件
ls -lh $HOME/*.cc $HOME/*.so
cat $HOME/fused_*.cc  # 查看生成的 C 代码
```

## 总结

**目标**：rvv_test.py 产生的 .so 可以利用 runtime_rvv 执行在 my_device（计算方法为 RVV）

**实现**：
1. ✅ rvv_test.py 使用 `my_device` target，生成 C codegen（不预编译）
2. ✅ runtime_rvv 加载 .so 提取 C codegen
3. ✅ my_device Module 的 GetFunction 被调用
4. ✅ my_deviceWrappedFunc 的 operator() 被调用
5. ✅ JIT 编译器根据 `TVM_RVV_COMPILER` 使用 riscv64-g++ + RVV 选项
6. ✅ 生成包含 RVV 指令的机器码
7. ✅ 在 my_device 设备上执行

**关键**：不使用 `fcompile` 预编译，让 C codegen 保留在 .so 中，运行时 JIT 编译！

