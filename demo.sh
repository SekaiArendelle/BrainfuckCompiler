#!/bin/bash

# Brainfuck LLVM 编译器演示脚本

set -e

echo "=== Brainfuck LLVM 编译器演示 ==="
echo

# 检查编译器
if [ ! -f "build/bin/bfc" ]; then
    echo "编译器未找到，请先运行:"
    echo "  ./build.sh"
    exit 1
fi

COMPILER="./build/bin/bfc"

echo "🚀 欢迎使用Brainfuck LLVM编译器!"
echo
echo "这个演示将展示编译器的各种功能。"
echo
echo "按回车键继续..."
read
n
clear
echo "=== 演示1: 基本编译 ==="
echo
echo "我们将编译经典的Hello World程序:"
echo
cat examples/hello.bf
echo
echo "编译命令:"
echo "$COMPILER -i examples/hello.bf -o hello"
echo
$COMPILER -i examples/hello.bf -o hello
echo
echo "运行结果:"
./hello
echo
echo "✓ 基本编译演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示2: 统计信息 ==="
echo
echo "查看编译统计信息:"
echo
echo "命令: $COMPILER -i examples/hello.bf -o hello -s"
echo
$COMPILER -i examples/hello.bf -o hello -s
echo
echo "✓ 统计信息显示演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示3: JIT模式 ==="
echo
echo "使用JIT模式直接执行，无需生成文件:"
echo
echo "命令: $COMPILER -i examples/hello.bf -j"
echo
$COMPILER -i examples/hello.bf -j
echo
echo "✓ JIT模式演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示4: 优化编译 ==="
echo
echo "启用LLVM优化进行编译:"
echo
echo "命令: $COMPILER -i examples/hello.bf -o hello_opt -O -s"
echo
$COMPILER -i examples/hello.bf -o hello_opt -O -s
echo
echo "运行优化版本:"
./hello_opt
echo
echo "✓ 优化编译演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示5: 不同程序类型 ==="
echo
echo "测试加法程序:"
echo
echo "源代码:"
cat examples/add.bf
echo
echo "编译并运行:"
$COMPILER -i examples/add.bf -o add_prog -s
./add_prog
echo
echo "✓ 加法程序演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示6: 内存配置 ==="
echo
echo "使用更大的内存编译:"
echo
echo "命令: $COMPILER -i examples/hello.bf -o hello_big -m 100000 -s"
echo
$COMPILER -i examples/hello.bf -o hello_big -m 100000 -s
echo
echo "运行:"
./hello_big
echo
echo "✓ 大内存配置演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示7: 错误处理 ==="
echo
echo "测试错误处理 - 括号不匹配:"
echo
echo "测试代码: +++[>+++<-]++]"
echo
echo "命令: $COMPILER -i /dev/stdin -o test_error"
echo
echo "$COMPILER -i /dev/stdin -o test_error << 'EOF'"
echo "+++[>+++<-]++]"
echo "EOF"
echo
$COMPILER -i /dev/stdin -o test_error << 'EOF'
+++[>+++<-]++]
EOF
echo
echo "✓ 错误处理演示完成"
echo
echo "按回车键继续..."
read

clear
echo "=== 演示完成! ==="
echo
echo "🎉 所有演示功能都已展示完毕!"
echo
echo "总结:"
echo "✅ 基本编译功能"
echo "✅ JIT即时执行"
echo "✅ LLVM优化支持"
echo "✅ 详细统计信息"
echo "✅ 灵活配置选项"
echo "✅ 完善的错误处理"
echo
echo "清理演示文件..."
rm -f hello hello_opt hello_big add_prog test_error
echo
echo "感谢使用Brainfuck LLVM编译器!"
echo
echo "更多信息请查看 README.md"