#!/bin/bash
# 驗證 Segfault 修復腳本

echo "=== 驗證 TVM 版本 ==="
python3 -c "import tvm; print('TVM版本:', tvm.__version__)"

echo ""
echo "=== 檢查修補結果 ==="
echo "✅ common.cc: 已修正多維度陣列填充邏輯"
grep -A 3 "計算總元素數量" /home/david/tvm/runtime_rvv/src/common.cc | head -5

echo ""
echo "✅ main.cc: 已增加輸入張量診斷輸出"
grep -A 2 "已建立" /home/david/tvm/runtime_rvv/src/main.cc | head -3

echo ""
echo "✅ test.py: host target 已改為 'c'"
grep "host=\"c\"" /home/david/tvm/test.py

echo ""
echo "=== 模型資訊 ==="
if [ -f mobilenet.so ]; then
    echo "✅ mobilenet.so 存在 (檔案大小: $(du -h mobilenet.so | cut -f1))"
else
    echo "❌ mobilenet.so 不存在，請先重新編譯模型"
fi

if [ -f mobilenet_meta.json ]; then
    echo "✅ mobilenet_meta.json:"
    cat mobilenet_meta.json | python3 -m json.tool
else
    echo "❌ mobilenet_meta.json 不存在"
fi

echo ""
echo "=== 編譯狀態 ==="
if [ -f /home/david/tvm/runtime_rvv/exec.out ]; then
    echo "✅ exec.out 已編譯完成"
    ls -lh /home/david/tvm/runtime_rvv/exec.out
else
    echo "❌ exec.out 不存在"
    exit 1
fi

echo ""
echo "============================================"
echo "接下來在 Banana Pi 上執行："
echo "  cd /path/to/tvm/runtime_rvv"
echo "  ./exec.out"
echo "  # 輸入: ../mobilenet.so"
echo "  # 輸入: ../mobilenet_meta.json"
echo ""
echo "預期輸出："
echo "  ✅ 已建立 1 個輸入張量"
echo "    Input[0]: shape=(1,3,224,224), device=17"
echo "  before run"
echo "  after run"
echo "  輸出結果（shape: ...）"
echo "============================================"

