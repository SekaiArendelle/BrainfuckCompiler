#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <memory>
#include "BrainfuckCompiler.h"

/**
 * @brief 显示使用帮助
 */
void showUsage(const char* programName) {
    std::cout << "Brainfuck LLVM 编译器" << std::endl;
    std::cout << "用法: " << programName << " [选项]" << std::endl;
    std::cout << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -i, --input <文件>     输入Brainfuck源文件" << std::endl;
    std::cout << "  -o, --output <文件>    输出可执行文件名" << std::endl;
    std::cout << "  -m, --memory <大小>    内存大小（默认: 30000）" << std::endl;
    std::cout << "  -O, --optimize         启用优化" << std::endl;
    std::cout << "  -g, --debug            生成调试信息" << std::endl;
    std::cout << "  -j, --jit              JIT模式直接执行" << std::endl;
    std::cout << "  -s, --stats            显示编译统计信息" << std::endl;
    std::cout << "  -h, --help             显示帮助信息" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << programName << " -i hello.bf -o hello" << std::endl;
    std::cout << "  " << programName << " -i mandelbrot.bf -o mandelbrot -O -m 60000" << std::endl;
    std::cout << "  " << programName << " -i test.bf -j -s" << std::endl;
}

/**
 * @brief 读取文件内容
 */
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief 解析命令行参数
 */
struct CommandLineOptions {
    std::string inputFile;
    std::string outputFile = "a.out";
    size_t memorySize = 30000;
    bool enableOptimization = false;
    bool enableDebugInfo = false;
    bool enableJIT = false;
    bool showStats = false;
    bool showHelp = false;
};

CommandLineOptions parseCommandLine(int argc, char* argv[]) {
    CommandLineOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                options.inputFile = argv[++i];
            } else {
                throw std::runtime_error("缺少输入文件名参数");
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                options.outputFile = argv[++i];
            } else {
                throw std::runtime_error("缺少输出文件名参数");
            }
        } else if (arg == "-m" || arg == "--memory") {
            if (i + 1 < argc) {
                options.memorySize = std::stoul(argv[++i]);
            } else {
                throw std::runtime_error("缺少内存大小参数");
            }
        } else if (arg == "-O" || arg == "--optimize") {
            options.enableOptimization = true;
        } else if (arg == "-g" || arg == "--debug") {
            options.enableDebugInfo = true;
        } else if (arg == "-j" || arg == "--jit") {
            options.enableJIT = true;
        } else if (arg == "-s" || arg == "--stats") {
            options.showStats = true;
        } else if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
        } else {
            throw std::runtime_error("未知选项: " + arg);
        }
    }
    
    return options;
}

/**
 * @brief 显示编译统计信息
 */
void showStatistics(const BrainfuckCompiler& compiler) {
    auto stats = compiler.getStatistics();
    
    std::cout << "\n=== 编译统计信息 ===" << std::endl;
    std::cout << "指令使用统计:" << std::endl;
    
    const char* instructionNames[] = {">", "<", "+", "-", ".", ",", "[", "]"};
    const char* instructionDesc[] = {
        "指针右移", "指针左移", "字节递增", "字节递减",
        "输出", "输入", "循环开始", "循环结束"
    };
    
    size_t totalInstructions = 0;
    
    for (size_t i = 0; i < 8; ++i) {
        char instr = *instructionNames[i];
        if (stats.count(instr) > 0) {
            std::cout << "  '" << instr << "' (" << instructionDesc[i] << "): " 
                      << stats.at(instr) << " 次" << std::endl;
            totalInstructions += stats.at(instr);
        }
    }
    
    std::cout << "总指令数: " << totalInstructions << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行参数
        CommandLineOptions options = parseCommandLine(argc, argv);
        
        // 显示帮助
        if (options.showHelp) {
            showUsage(argv[0]);
            return 0;
        }
        
        // 检查必需参数
        if (options.inputFile.empty()) {
            std::cerr << "错误: 必须指定输入文件" << std::endl;
            std::cerr << "使用 '" << argv[0] << " --help' 查看用法" << std::endl;
            return 1;
        }
        
        // 读取源文件
        std::string sourceCode = readFile(options.inputFile);
        
        // 创建编译器
        BrainfuckCompiler compiler(options.memorySize);
        
        // 设置编译选项
        compiler.setOptimization(options.enableOptimization);
        compiler.setDebugInfo(options.enableDebugInfo);
        
        // 编译
        std::cout << "正在编译: " << options.inputFile << std::endl;
        std::cout << "内存大小: " << options.memorySize << " 字节" << std::endl;
        std::cout << "优化: " << (options.enableOptimization ? "启用" : "禁用") << std::endl;
        std::cout << "调试信息: " << (options.enableDebugInfo ? "启用" : "禁用") << std::endl;
        std::cout << "执行模式: " << (options.enableJIT ? "JIT" : "编译") << std::endl;
        
        bool success = compiler.compile(sourceCode, options.outputFile, options.enableJIT);
        
        if (!success) {
            std::cerr << "编译失败" << std::endl;
            return 1;
        }
        
        // 显示统计信息
        if (options.showStats) {
            showStatistics(compiler);
        }
        
        std::cout << "编译成功!" << std::endl;
        
        if (!options.enableJIT) {
            std::cout << "输出文件: " << options.outputFile << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}