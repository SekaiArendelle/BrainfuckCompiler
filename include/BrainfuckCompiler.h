#ifndef BRAINFUCK_COMPILER_H
#define BRAINFUCK_COMPILER_H

#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <map>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

/**
 * @class BrainfuckCompiler
 * @brief 将Brainfuck源代码编译为原生机器码的LLVM编译器
 *
 * 该类实现了完整的Brainfuck语言编译器，包括：
 * - 完整的8条Brainfuck指令支持
 * - LLVM IR生成
 * - 优化支持
 * - JIT执行
 * - 调试信息生成
 */
class BrainfuckCompiler {
public:
    /**
     * @brief 构造函数
     * @param memorySize 内存大小（默认30000个单元）
     */
    explicit BrainfuckCompiler(size_t memorySize = 30000);

    /**
     * @brief 析构函数
     */
    ~BrainfuckCompiler();

    /**
     * @brief 编译Brainfuck源代码
     * @param source 源代码字符串
     * @param outputFile 输出文件名（不含扩展名）
     * @param enableJIT 是否启用JIT模式直接执行
     * @return 编译成功返回true
     */
    bool compile(const std::string& source, const std::string& outputFile, bool enableJIT = false);

    /**
     * @brief 启用/禁用优化
     * @param enable 是否启用优化
     */
    void setOptimization(bool enable) {
        m_enableOptimization = enable;
    }

    /**
     * @brief 启用/禁用调试信息
     * @param enable 是否启用调试信息
     */
    void setDebugInfo(bool enable) {
        m_enableDebugInfo = enable;
    }

    /**
     * @brief 获取编译统计信息
     * @return 包含各指令使用次数的map
     */
    std::map<char, size_t> getStatistics() const {
        return m_statistics;
    }

private:
    // LLVM初始化
    void initializeLLVM();

    // IR生成主函数
    void generateIR(const std::string& source);

    // Brainfuck指令处理函数
    void handleIncrementPtr(); // > 指针递增
    void handleDecrementPtr(); // < 指针递减
    void handleIncrementByte(); // + 字节递增
    void handleDecrementByte(); // - 字节递减
    void handleOutput(); // . 输出
    void handleInput(); // , 输入
    void handleLoopStart(size_t ip); // [ 循环开始
    void handleLoopEnd(size_t ip); // ] 循环结束

    // 辅助函数
    void createMainFunction();
    void allocateMemory();
    void setupRuntimeFunctions();
    void optimizeModule();
    void emitObjectFile(const std::string& outputFile);
    void executeJIT();

    // 错误处理
    bool checkBrackets(const std::string& source);
    void reportError(const std::string& message);

    // 调试信息生成
    void createDebugInfo();
    void finalizeDebugInfo();

    // 成员变量
    size_t m_memorySize; // 内存大小
    bool m_enableOptimization; // 是否启用优化
    bool m_enableDebugInfo; // 是否启用调试信息
    std::map<char, size_t> m_statistics; // 指令统计

    // LLVM相关成员
    std::unique_ptr<llvm::LLVMContext> m_context;
    std::unique_ptr<llvm::Module> m_module;
    std::unique_ptr<llvm::IRBuilder<>> m_builder;
    std::unique_ptr<llvm::DIBuilder> m_diBuilder;

    // IR值
    llvm::Value* m_memoryArray; // 内存数组
    llvm::Value* m_dataPtr; // 数据指针
    llvm::Function* m_mainFunction; // main函数

    // 运行时函数
    llvm::Function* m_putcharFunc; // putchar函数
    llvm::Function* m_getcharFunc; // getchar函数

    // 循环处理
    std::stack<llvm::BasicBlock*> m_loopStartBlocks;
    std::stack<llvm::BasicBlock*> m_loopEndBlocks;

    // 源代码位置跟踪
    size_t m_currentIP; // 当前指令指针
};

#endif // BRAINFUCK_COMPILER_H
