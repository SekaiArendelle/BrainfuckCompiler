# Brainfuck LLVM 编译器

一个功能完整的Brainfuck语言编译器，使用LLVM作为后端生成原生机器码。

## 特性

- ✅ 完整的Brainfuck 8条指令支持
- ✅ LLVM IR生成和优化
- ✅ JIT即时执行模式
- ✅ 调试信息生成
- ✅ 语法错误检测
- ✅ 编译统计信息
- ✅ 可配置内存大小
- ✅ 跨平台支持

## 项目结构

```
BrainfuckCompiler/
├── include/
│   └── BrainfuckCompiler.h    # 编译器头文件
├── src/
│   ├── BrainfuckCompiler.cpp  # 编译器实现
│   └── main.cpp              # 命令行接口
├── examples/                  # 示例程序
│   ├── hello.bf              # Hello World
│   ├── cat.bf                # 文件复制
│   ├── add.bf                # 加法运算
│   └── counter.bf            # 计数器
├── CMakeLists.txt            # 构建配置
└── README.md                 # 文档
```

## 构建要求

- C++17兼容的编译器
- LLVM 14+ 开发包
- CMake 3.10+

### Ubuntu/Debian

```bash
sudo apt-get install llvm-14-dev clang cmake build-essential
```

### macOS

```bash
brew install llvm@14 cmake
```

## 构建步骤

1. 克隆项目
```bash
git clone <repository-url>
cd BrainfuckCompiler
```

2. 创建构建目录
```bash
mkdir build
cd build
```

3. 配置CMake
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

4. 编译
```bash
make -j$(nproc)
```

## 使用方法

### 基本用法

```bash
# 编译Brainfuck源文件
./bin/bfc -i examples/hello.bf -o hello

# 运行生成的可执行文件
./hello
```

### 命令行选项

```
用法: bfc [选项]

选项:
  -i, --input <文件>     输入Brainfuck源文件 (必需)
  -o, --output <文件>    输出可执行文件名 (默认: a.out)
  -m, --memory <大小>    内存大小，单位字节 (默认: 30000)
  -O, --optimize         启用LLVM优化
  -g, --debug            生成调试信息
  -j, --jit              JIT模式直接执行
  -s, --stats            显示编译统计信息
  -h, --help             显示帮助信息
```

### 使用示例

1. **基本编译**
```bash
./bin/bfc -i examples/hello.bf -o hello
./hello                    # 输出: Hello World!
```

2. **启用优化**
```bash
./bin/bfc -i examples/hello.bf -o hello_opt -O
```

3. **JIT模式**
```bash
./bin/bfc -i examples/hello.bf -j
```

4. **显示统计信息**
```bash
./bin/bfc -i examples/hello.bf -o hello -s
```

5. **自定义内存大小**
```bash
./bin/bfc -i examples/mandelbrot.bf -o mandelbrot -m 60000
```

## 示例程序

### Hello World (`examples/hello.bf`)
```brainfuck
++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.
```

### 文件复制 (`examples/cat.bf`)
```brainfuck
,[.,]
```

### 加法运算 (`examples/add.bf`)
```brainfuck
++        Cell #0 = 2
> +++++   Cell #1 = 5
[         Start your loops with your cell pointer on the loop counter
  -       Decrement the loop counter
  < +     Add 1 to Cell #0
  >       Move back to the loop counter
]
< .       Print out Cell #0's value
```

## 技术实现

### 内存模型
- 使用30,000个单元的字节数组（可配置）
- 数据指针初始位置在数组中间
- 环绕式边界检查

### LLVM IR生成
- 使用`AllocaInst`分配内存数组和指针
- `GetElementPtr`指令处理指针移动
- `load/add/store`序列处理字节操作
- `br`和`phi`节点实现循环
- 调用`putchar`/`getchar`处理I/O

### 优化
- 启用LLVM标准优化流水线
- 包括指令合并、重关联、GVN、CFG简化
- 支持-O0到-O3优化级别

### JIT执行
- 使用LLVM MCJIT/OrcJIT
- 直接执行生成的机器码
- 无需生成中间文件

## 调试支持

### 生成调试信息
```bash
./bin/bfc -i examples/hello.bf -o hello -g
```

### 使用GDB调试
```bash
gdb ./hello
(gdb) run
```

## 性能对比

| 实现方式 | 执行速度 | 内存使用 | 特点 |
|---------|---------|---------|------|
| 解释器 | 慢 | 低 | 简单，易调试 |
| JIT编译 | 中等 | 中等 | 快速执行，无需文件 |
| AOT编译 | 快 | 高 | 最佳性能，生成可执行文件 |

## 错误处理

### 语法错误
- 括号不匹配检测
- 详细的错误位置报告
- 友好的错误消息

### 运行时错误
- 内存访问边界检查
- I/O错误处理
- LLVM IR验证

## 扩展功能

### 统计信息
编译器会统计各指令使用次数：
```
=== 编译统计信息 ===
指令使用统计:
  '>' (指针右移): 8 次
  '<' (指针左移): 8 次
  '+' (字节递增): 43 次
  '-' (字节递减): 10 次
  '.' (输出): 12 次
  '[' (循环开始): 3 次
  ']' (循环结束): 3 次
总指令数: 87
```

### 内存配置
支持自定义内存大小：
```bash
./bin/bfc -i program.bf -o program -m 100000  # 100KB内存
```

## 开发指南

### 代码结构
- `BrainfuckCompiler.h/cpp` - 核心编译器类
- `main.cpp` - 命令行接口
- 模块化设计，易于扩展

### 添加新功能
1. 在头文件中声明新方法
2. 在实现文件中添加功能
3. 更新命令行接口
4. 添加测试用例

## 许可证

MIT License - 详见LICENSE文件

## 贡献

欢迎提交Issue和Pull Request！

## 致谢

- LLVM项目提供的优秀编译器基础设施
- Brainfuck语言的发明者Urban Müller
- 开源社区的支持