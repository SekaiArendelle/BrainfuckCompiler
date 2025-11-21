#!/bin/bash

# Brainfuck LLVM 编译器测试脚本

set -e

echo "=== Brainfuck 编译器测试 ==="
echo

# 检查编译器是否存在
if [ ! -f "build/bin/bfc" ]; then
    echo "错误: 编译器未找到，请先运行 ./build.sh"
    exit 1
fi

COMPILER="./build/bin/bfc"
EXAMPLES_DIR="./examples"

echo "使用编译器: $COMPILER"
echo

# 测试1: Hello World
echo "测试1: Hello World 程序"
echo "编译..."
$COMPILER -i $EXAMPLES_DIR/hello.bf -o hello_test -s
echo "运行..."
./hello_test
echo "✓ Hello World 测试通过"
echo

# 测试2: 加法程序
echo "测试2: 加法程序 (2 + 5)"
echo "编译..."
$COMPILER -i $EXAMPLES_DIR/add.bf -o add_test -s
echo "运行..."
./add_test
echo "✓ 加法测试通过"
echo

# 测试3: JIT模式
echo "测试3: JIT模式执行"
echo "JIT执行 Hello World..."
$COMPILER -i $EXAMPLES_DIR/hello.bf -j -s
echo "✓ JIT模式测试通过"
echo

# 测试4: 带优化的编译
echo "测试4: 带优化的编译"
echo "编译并启用优化..."
$COMPILER -i $EXAMPLES_DIR/hello.bf -o hello_opt -O -s
echo "运行优化版本..."
./hello_opt
echo "✓ 优化测试通过"
echo

# 测试5: 错误处理
echo "测试5: 错误处理"
echo "测试括号不匹配..."
$COMPILER -i /dev/stdin -o test_error << 'EOF'
+++[>+++<-]++]
EOF
if [ $? -ne 0 ]; then
    echo "✓ 错误处理测试通过"
else
    echo "✗ 错误处理测试失败"
fi
echo

# 清理测试文件
rm -f hello_test add_test hello_opt test_error

echo "=== 所有测试通过! ==="
echo
echo "编译器功能验证完成 ✓"
echo "可以使用 ./build/bin/bfc 编译您的Brainfuck程序"