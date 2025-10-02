# 快速开始

## 一、编译 runtime_rvv

```bash
cd /home/david/tvm_rvv/runtime_rvv
make clean && make build
```

## 二、生成模型（使用修改后的 rvv_test.py）

```bash
cd /home/david/tvm_rvv
python3 rvv_test.py mobilenet
```

**重要**：确保 rvv_test.py 使用：
```python
ex.export_library(OUT_SO)  # 不带 fcompile 参数
```

## 三、创建 metadata

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

**注意**：device 必须是 `"my_device"`

## 四、运行

### 本地测试（g++）
```bash
cd runtime_rvv
./exec.out
# 输入：../mobilenet.so
# 输入：../mobilenet_meta.json
```

### RVV 模式（riscv64-g++）
```bash
export TVM_RVV_COMPILER="riscv64-linux-gnu-g++"
cd runtime_rvv
./exec.out
# 输入：../mobilenet.so
# 输入：../mobilenet_meta.json
```

## 预期输出

```
請輸入模型 .so 路徑：../mobilenet.so
請輸入模型 Json 路徑：../mobilenet_meta.json
Data: <C codegen 源码>
RVV compile command: riscv64-linux-gnu-g++ -O3 -fPIC -shared -march=rv64gcv -mabi=lp64d -o ...
輸出結果（shape: 1x1000, dtype: float32):
0.123 0.456 ...
```

## 关键检查点

✅ **看到 "Data: ..."** → C codegen 成功提取  
✅ **看到 "RVV compile command"** → JIT 编译启动  
✅ **包含 "-march=rv64gcv"** → RVV 支持启用  
✅ **看到输出结果** → 执行成功

## 常见问题

### Q: 没有看到 "Data: ..."
**A**: .so 被预编译了，检查 rvv_test.py 是否使用了 fcompile

### Q: 编译失败
**A**: 检查编译器是否安装：`which riscv64-linux-gnu-g++`

### Q: device 不存在错误
**A**: metadata 中 device 应该是 `"my_device"`

## 详细文档

- **README.md** - 完整使用说明
- **FLOW.md** - 详细流程说明
- **VERIFY.md** - 验证步骤
- **FINAL_SUMMARY.md** - 项目总结

