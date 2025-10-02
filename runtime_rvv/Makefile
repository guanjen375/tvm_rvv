##############################################################################
# TVM Runtime for my_device (LLVM Mode)
#
# 此 runtime 專用於執行透過 LLVM backend 編譯的 my_device 模組
# - my_device 使用 LLVM 生成 RISC-V RVV 機器碼
# - Runtime 直接從 .so 調用已編譯的 kernel（不需要動態編譯）
# - 只包含必要的設備 API（記憶體管理、設備抽象）
##############################################################################

OBJ=./obj
EXEC=./exec.out

# 使用 RISC-V 交叉編譯器（在 Banana Pi 上執行）
CC=riscv64-linux-gnu-g++
LDFLAGS=-ldl -lpthread -rdynamic -lffi
INCLUDE=$(addprefix -I,./include ./include/dlpack/include ./include/support ./include/picojson)

# 所有來源檔
# 只包含 LLVM 模式需要的檔案：
# - my_device_device_api.cc：提供設備記憶體管理和設備抽象
# - 不包含 my_device_module.cc：LLVM 模式不需要動態編譯
SRC = \
  src/main.cc \
  src/module.cc \
  src/registry.cc \
  src/c_runtime_api.cc \
  src/object.cc \
  src/logging.cc \
  src/ndarray.cc \
  src/workspace_pool.cc \
  src/file_utils.cc \
  src/library_module.cc \
  src/dso_library.cc \
  src/thread_pool.cc \
  src/threading_backend.cc \
  src/profiling.cc \
  src/nvtx.cc \
  src/cpu_device_api.cc \
  src/common.cc \
  src/boxed_primitive.cc \
  $(wildcard src/memory/*.cc) \
  src/my_device/my_device_device_api.cc \
  $(filter-out %/cuda/% %/hexagon/%, $(wildcard src/relax_vm/*.cc))

# 把 SRC 改成對應的 obj 檔
OBJECTS = $(patsubst src/%.cc,$(OBJ)/%.o,$(SRC))

# 預設目標
build: $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LDFLAGS)
	@echo Done!

# 單個 .o 編譯規則
$(OBJ)/%.o: src/%.cc
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDE) $(CFLAGS) -std=c++17 -w -c -o $@ $<

run:
	$(EXEC)

clean:
	rm -rf $(OBJ) ./exec.out

