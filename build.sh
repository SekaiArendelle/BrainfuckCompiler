#!/bin/bash

# Brainfuck LLVM 编译器构建脚本

set -e

echo "=== Brainfuck LLVM 编译器构建脚本 ==="
echo

# 检查依赖
echo "检查依赖..."

# 检查CMake
if ! command -v cmake &> /dev/null; then
    echo "错误: 未找到cmake，请先安装CMake 3.10+"
    exit 1
fi

# 检查LLVM-config
if ! command -v llvm-config &> /dev/null; then
    echo "错误: 未找到llvm-config，请先安装LLVM 14+开发包"
    echo "Ubuntu/Debian: sudo apt-get install llvm-14-dev"
    echo "macOS: brew install llvm@14"
    exit 1
fi

# 检查编译器
if ! command -v clang &> /dev/null && ! command -v g++ &> /dev/null; then
    echo "错误: 未找到C++编译器，请先安装clang或g++"
    exit 1
fi

echo "依赖检查通过 ✓"
echo

# 创建构建目录
echo "创建构建目录..."
if [ -d "build" ]; then
    echo "清理旧的构建目录..."
    rm -rf build
fi
mkdir build
cd build

# 配置CMake
echo "配置CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo
echo "编译项目..."
make -j$(nproc 2>/dev/null || echo 4)

echo
echo "=== 构建完成 ==="
echo "可执行文件位置: ./bin/bfc"
echo
echo "运行测试:"
echo "  ./bin/bfc --help"
echo "  ./bin/bfc -i ../examples/hello.bf -o hello -s"
echo
echo "安装到系统:"
echo "  sudo make install"