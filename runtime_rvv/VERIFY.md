# 验证文档

## 完整流程验证

### 1. 编译 runtime_rvv

```bash
cd /home/david/tvm_rvv/runtime_rvv
make clean
make build
ls -lh exec.out
```

**预期输出**：
```
-rwxrwxr-x 1 david david 10M ... exec.out
Done!
```

### 2. 生成测试模型

```bash
cd /home/david/tvm_rvv

# 创建简单测试模型
cat > test_simple.py << 'EOF'
import tvm
from tvm import relax
from tvm.script import ir as I, relax as R
import numpy as np

# 简单的 add kernel
@I.ir_module
class TestModule:
    @R.function
    def main(x: R.Tensor((2, 3), "float32"), y: R.Tensor((2, 3), "float32")) -> R.Tensor((2, 3), "float32"):
        with R.dataflow():
            z = R.add(x, y)
            R.output(z)
        return z

mod = TestModule
target = "my_device"
host = "llvm"

with tvm.target.Target(target, host=host):
    mod = relax.transform.LegalizeOps()(mod)
    ex = relax.build(mod, target=tvm.target.Target(target, host=host))

ex.export_library("test_add.so")
print("✓ 已生成 test_add.so（包含 my_device C codegen）")
EOF

python3 test_simple.py
```

### 3. 创建 metadata

```bash
cat > test_add_meta.json << 'EOF'
{
  "inputs": [
    {
      "name": "x",
      "shape": [2, 3],
      "dtype": "float32",
      "device": "my_device"
    },
    {
      "name": "y",
      "shape": [2, 3],
      "dtype": "float32",
      "device": "my_device"
    }
  ]
}
EOF
```

### 4. 测试本地执行（g++）

```bash
cd runtime_rvv
./exec.out
# 输入：../test_add.so
# 输入：../test_add_meta.json
```

**预期行为**：
1. 显示 C codegen 内容（"Data: ..."）
2. 显示编译命令（"RVV compile command: g++ ..."）
3. JIT 编译
4. 显示输出结果

### 5. 测试 RVV 编译器

```bash
export TVM_RVV_COMPILER="riscv64-linux-gnu-g++"
cd runtime_rvv
./exec.out
# 输入：../test_add.so
# 输入：../test_add_meta.json
```

**预期行为**：
1. 编译命令包含 `-march=rv64gcv -mabi=lp64d`
2. 使用 riscv64-linux-gnu-g++ 编译

### 6. 检查临时文件

```bash
ls -lh $HOME/*.cc $HOME/*.so
```

**预期**：看到 JIT 编译生成的临时文件

### 7. 查看 C codegen

```bash
cat $HOME/fused_add*.cc
```

**预期**：看到 TVM 生成的 C 代码

## 关键验证点

### ✅ 1. my_device 设备注册

```bash
strings runtime_rvv/exec.out | grep "device_api.my_device"
```

**预期**：找到注册字符串

### ✅ 2. JIT 编译器代码

```bash
grep -n "TVM_RVV_COMPILER" runtime_rvv/src/my_device/my_device_common.cc
```

**预期**：第 120 行

### ✅ 3. RVV 编译选项

```bash
grep -n "march=rv64gcv" runtime_rvv/src/my_device/my_device_common.cc
```

**预期**：第 128 行

### ✅ 4. 主程序使用 my_device

```bash
grep -n "kDLmy_device" runtime_rvv/src/main.cc
```

**预期**：第 34 行

## 流程图验证

```
[rvv_test.py] 
    ↓ 生成 my_device C codegen
[mobilenet.so] (包含 C 字符串)
    ↓ runtime_rvv 加载
[my_device Module]
    ↓ GetFunction("kernel_name")
[my_deviceWrappedFunc]
    ↓ operator() 调用
[compile_model_dynamic()] ← 检查 TVM_RVV_COMPILER
    ↓ 写入 $HOME/kernel.cc
    ↓ riscv64-g++ -march=rv64gcv ...
    ↓ 生成 $HOME/kernel.so
    ↓ dlopen + dlsym
[DynamicKernel] 
    ↓ ffi_call
[RVV 指令执行] ✓
```

## 错误检查

### 如果看到 "Data: (空)"
**问题**：.so 没有包含 C codegen  
**原因**：rvv_test.py 使用了 fcompile 预编译  
**解决**：确认 rvv_test.py 使用 `ex.export_library(OUT_SO)` 不带 fcompile

### 如果没有进入 compile_model_dynamic
**问题**：.so 包含的是机器码，不是 C codegen  
**解决**：重新用修改后的 rvv_test.py 生成

### 如果编译失败
**问题**：编译器不存在或选项错误  
**解决**：
```bash
# 检查编译器
which riscv64-linux-gnu-g++

# 或使用本地编译器
unset TVM_RVV_COMPILER
```

## 最小成功案例

```bash
# 1. 编译 runtime
cd /home/david/tvm_rvv/runtime_rvv
make build

# 2. 生成简单模型（使用修改后的 rvv_test.py）
cd /home/david/tvm_rvv
python3 -c "
import tvm
from tvm import relax
from tvm.script import ir as I, relax as R

@I.ir_module
class TestModule:
    @R.function
    def main(x: R.Tensor((2, 3), 'float32')) -> R.Tensor((2, 3), 'float32'):
        with R.dataflow():
            z = R.multiply(x, R.const(2.0, 'float32'))
            R.output(z)
        return z

mod = TestModule
ex = relax.build(mod, target='my_device', host='llvm')
ex.export_library('test.so')
print('✓ 生成 test.so')
"

# 3. 创建 metadata
cat > test_meta.json << 'EOF'
{"inputs": [{"name": "x", "shape": [2, 3], "dtype": "float32", "device": "my_device"}]}
EOF

# 4. 运行（本地测试）
cd runtime_rvv
./exec.out
# 输入：../test.so
# 输入：../test_meta.json
```

## 成功标志

1. ✅ 看到 "Data: ..." 输出（C codegen 字符串）
2. ✅ 看到 "RVV compile command: ..." 输出
3. ✅ 编译成功（返回码 0）
4. ✅ dlopen 成功
5. ✅ 看到计算结果输出

## TVM 版本确认

```bash
# 需要先构建 TVM 或设置正确的 PYTHONPATH
export PYTHONPATH=/home/david/tvm_rvv/python:$PYTHONPATH
export LD_LIBRARY_PATH=/home/david/tvm_rvv/build:$LD_LIBRARY_PATH

python3 -c "import tvm; print(tvm.__version__)"
# 预期：0.20.dev...
```
