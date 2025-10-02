OBJ=./obj
EXEC=./exec.out

CC=g++
LDFLAGS=-ldl -lpthread -rdynamic -lffi
INCLUDE=$(addprefix -I,./include ./include/dlpack/include ./include/support ./include/picojson)

# 所有來源檔
# 注意：不包含 my_device_module.cc（LLVM mode 不需要）
# 只包含 my_device_device_api.cc（提供設備抽象）
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
	$(CC) $(INCLUDE) -std=c++17 -w -c -o $@ $<

run:
	$(EXEC)

clean:
	rm -rf $(OBJ) ./exec.out

