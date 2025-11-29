#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
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
 * @brief LLVM compiler that compiles Brainfuck source code to native machine code
 *
 * This class implements a complete Brainfuck language compiler, including:
 * - Complete support for 8 Brainfuck instructions
 * - LLVM IR generation
 * - Optimization support
 * - JIT execution
 * - Debug information generation
 */
class BrainfuckCompiler {
public:
    /**
     * @brief Constructor
     * @param memorySize Memory size (default 30000 cells)
     */
    BrainfuckCompiler(std::size_t memorySize, bool enableOptimization)
        : m_memorySize(memorySize),
          m_enableOptimization(enableOptimization),
          m_enableDebugInfo(false),
          m_currentIP(0) {
        // Initialize LLVM
        initializeLLVM();
    }

    /**
     * @brief Destructor
     */
    ~BrainfuckCompiler();

    /**
     * @brief Compile Brainfuck source code
     * @param source Source code string
     * @param outputFile Output filename (without extension)
     * @param enableJIT Whether to enable JIT mode for direct execution
     * @return Returns true if compilation successful
     */
    bool compile(std::string_view source, std::string_view outputFile, bool enableJIT = false);

    /**
     * @brief Enable/disable debug information
     * @param enable Whether to enable debug information
     */
    void setDebugInfo(bool enable) {
        m_enableDebugInfo = enable;
    }

    /**
     * @brief Get compilation statistics
     * @return Map containing instruction usage counts
     */
    std::map<char, std::size_t> getStatistics() const {
        return m_statistics;
    }

private:
    // LLVM initialization
    void initializeLLVM();

    // IR generation main function
    void generateIR(std::string_view source);

    // Brainfuck instruction handling functions
    void handleIncrementPtr(); // > Pointer increment
    void handleDecrementPtr(); // < Pointer decrement
    void handleIncrementByte(); // + Byte increment
    void handleDecrementByte(); // - Byte decrement
    void handleOutput(); // . Output
    void handleInput(); // , Input
    void handleLoopStart(std::size_t ip); // [ Loop start
    void handleLoopEnd(std::size_t ip); // ] Loop end

    // Helper functions
    void createMainFunction();
    void allocateMemory();
    void setupRuntimeFunctions();
    void optimizeModule();
    void emitObjectFile(std::string_view outputFile);
    void executeJIT();

    // Error handling
    bool checkBrackets(std::string_view source);
    void reportError(std::string_view message);

    // Debug information generation
    void createDebugInfo();
    void finalizeDebugInfo();

    // Member variables
    std::size_t m_memorySize; // Memory size
    bool m_enableOptimization; // Whether optimization is enabled
    bool m_enableDebugInfo; // Whether debug info is enabled
    std::map<char, std::size_t> m_statistics; // Instruction statistics

    // LLVM related members
    std::unique_ptr<llvm::LLVMContext> m_context;
    std::unique_ptr<llvm::Module> m_module;
    std::unique_ptr<llvm::IRBuilder<>> m_builder;
    std::unique_ptr<llvm::DIBuilder> m_diBuilder;

    // IR values
    llvm::Value* m_memoryArray; // Memory array
    llvm::Value* m_dataPtr; // Data pointer
    llvm::Function* m_mainFunction; // Main function

    // Runtime functions
    llvm::Function* m_putcharFunc; // putchar function
    llvm::Function* m_getcharFunc; // getchar function

    // Loop handling
    std::stack<llvm::BasicBlock*> m_loopStartBlocks;
    std::stack<llvm::BasicBlock*> m_loopEndBlocks;

    // Source location tracking
    std::size_t m_currentIP; // Current instruction pointer
};
