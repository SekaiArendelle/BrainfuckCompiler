# Brainfuck LLVM 编译器 - 简单Makefile
# 注意: 推荐使用CMake进行完整构建，此为简化的Makefile

CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -I./include
TARGET = bfc

# 获取LLVM配置
LLVM_CONFIG = llvm-config
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS = $(shell $(LLVM_CONFIG) --libs core executionengine mcjit orcjit support native irreader object bitwriter transformutils scalar instcombine analysis)

SOURCES = src/BrainfuckCompiler.cpp src/main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $^ -o $@ $(LLVM_LDFLAGS) $(LLVM_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# 调试构建
debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -I./include
debug: $(TARGET)

# 快速测试
test: $(TARGET)
	./$(TARGET) -i examples/hello.bf -o hello_test
	./hello_test
	rm -f hello_test

help:
	@echo "Brainfuck LLVM 编译器 Makefile"
	@echo ""
	@echo "用法:"
	@echo "  make          - 构建编译器"
	@echo "  make clean    - 清理构建文件"
	@echo "  make install  - 安装到/usr/local/bin"
	@echo "  make debug    - 构建调试版本"
	@echo "  make test     - 快速测试"
	@echo ""
	@echo "推荐使用CMake进行完整构建:"
	@echo "  mkdir build && cd build"
	@echo "  cmake .. && make"