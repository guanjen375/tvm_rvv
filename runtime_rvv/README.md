# TVM Runtime for my_device (LLVM Mode)

在 RISC-V 設備（Banana Pi）上執行 LLVM 編譯的 my_device 模組。

## 編譯

在 x86 開發機上：

```bash
cd /home/david/tvm/runtime_rvv
make clean
make build
```

檢查：
```bash
file exec.out  # 應顯示: RISC-V 64-bit
```

## 執行

### 準備檔案
- `exec.out` - Runtime 執行檔
- `mobilenet.so` - 模型檔案（RISC-V RVV）
- `mobilenet.json` - 模型 metadata

### 在 Banana Pi 上運行

```bash
# 如需環境設定
source env.sh

# 執行
./exec.out
```

互動式輸入：
```
請輸入模型 .so 路徑：mobilenet.so
請輸入模型 Json 路徑：mobilenet.json
```

### 預期輸出

```
✅ 已建立 1 個輸入張量
  Input[0]: shape=(1,3,224,224), device=14
before run
after run
輸出結果（shape: 1x1000）
[推理結果數據...]
```

## Troubleshooting

**Core Dump**  
→ 檢查架構：`file exec.out`（必須是 RISC-V）

**找不到符號**  
→ 檢查 .so：`nm -D mobilenet.so | grep kernel`
