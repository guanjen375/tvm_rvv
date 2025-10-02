# Runtime for RVV (my_device = LLVM/RVV)

## 设计理念

**my_device 作为设备抽象，行为等同 RVV**

```
my_device target → LLVM backend → RISC-V RVV 机器码
```

## 完整流程

```
编译时 (x86 + rvv_test.py):
  ONNX → Relax → my_device target → LLVM codegen → RISC-V RVV 机器码
  → 交叉编译链接 → .so

运行时 (Banana Pi + runtime_rvv):
  加载 .so → 标准 TVM runtime → 直接执行 RVV 机器码
```

## 与纯 LLVM target 的区别

| 特性 | 纯 LLVM target | my_device (LLVM backend) |
|------|---------------|-------------------------|
| 设备类型 | CPU (host) | my_device (device) |
| VM 初始化 | kDLCPU | kDLmy_device |
| metadata device | "cpu" | "my_device" |
| 代码生成 | LLVM | LLVM（相同） |
| 机器码 | 相同 | 相同 |
| 设备抽象 | 无独立抽象 | 有独立设备抽象 |

**关键**：my_device 提供独立的设备抽象层，但内部使用 LLVM 生成 RVV 机器码。

## 使用方法

### 步骤 1：生成模型（x86）

```bash
cd /home/david/tvm_rvv
python3 rvv_test.py mobilenet
```

**输出**：
- `mobilenet.so` - 包含 RISC-V RVV 机器码（交叉编译）

### 步骤 2：创建 metadata

```bash
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
```

**注意**：device 必须是 `"my_device"`（不是 "cpu"）

### 步骤 3：传输到 Banana Pi

```bash
scp mobilenet.so mobilenet_meta.json user@banana-pi:~
scp -r runtime_rvv user@banana-pi:~
```

### 步骤 4：在 Banana Pi 上运行

```bash
# 编译 runtime（首次）
cd runtime_rvv
make build

# 运行
./exec.out
# 输入：../mobilenet.so
# 输入：../mobilenet_meta.json
```

## 编译

```bash
cd /home/david/tvm_rvv/runtime_rvv
make clean
make build
```

## 目录结构

```
runtime_rvv/
├── Makefile              # 编译系统
├── exec.out              # 可执行文件
├── README.md             # 本文档
├── include/              # TVM runtime headers
├── support/              # 支持库
└── src/
    ├── main.cc           # 主程序（使用 kDLmy_device）
    ├── common.cc         # 辅助函数
    ├── my_device/        # my_device 设备 API（不再需要 JIT 编译）
    ├── memory/           # 内存管理
    └── relax_vm/         # VM 实现
```

## 注意事项

1. **交叉编译**：rvv_test.py 使用 `riscv64-linux-gnu-g++` 交叉编译
2. **目标架构**：生成的 .so 只能在 RISC-V 设备运行
3. **设备抽象**：使用 my_device 作为设备（不是 CPU）

## TVM 版本

- TVM: 0.20
- LLVM: 0.18
- Python: 3.11.13
