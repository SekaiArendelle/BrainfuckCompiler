#include <optional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// LLVM headers
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

// MCJIT headers
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>

// Debug info headers
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>

#include "BrainfuckCompiler.h"

using namespace llvm;

BrainfuckCompiler::BrainfuckCompiler(size_t memorySize)
    : m_memorySize(memorySize),
      m_enableOptimization(true),
      m_enableDebugInfo(false),
      m_currentIP(0) {
    // Initialize LLVM
    initializeLLVM();
}

BrainfuckCompiler::~BrainfuckCompiler() {
    // Clean up resources
    if (m_diBuilder) {
        finalizeDebugInfo();
    }
}

void BrainfuckCompiler::initializeLLVM() {
    // Initialize LLVM targets
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    // Create LLVM context and module
    m_context = std::make_unique<LLVMContext>();
    m_module = std::make_unique<Module>("brainfuck_module", *m_context);
    m_builder = std::make_unique<IRBuilder<>>(*m_context);

    // Set target triple
    auto targetTriple = sys::getDefaultTargetTriple();
    m_module->setTargetTriple(llvm::Triple(targetTriple));
}

bool BrainfuckCompiler::compile(const std::string& source, const std::string& outputFile, bool enableJIT) {
    try {
        // Check bracket matching
        if (!checkBrackets(source)) {
            return false;
        }

        // 重置统计
        m_statistics.clear();

        // 创建主函数和分配内存
        createMainFunction();
        allocateMemory();
        setupRuntimeFunctions();

        // 生成调试信息（如果启用）
        if (m_enableDebugInfo) {
            createDebugInfo();
        }

        // 生成IR
        generateIR(source);

        // 验证IR
        if (verifyModule(*m_module, &errs())) {
            reportError("Generated IR is invalid");
            return false;
        }

        // 应用优化
        if (m_enableOptimization) {
            optimizeModule();
        }

        // 输出IR（调试用）
        // m_module->print(errs(), nullptr);

        if (enableJIT) {
            // JIT模式直接执行
            executeJIT();
        } else {
            // 生成目标文件
            emitObjectFile(outputFile);
        }

        return true;

    } catch (const std::exception& e) {
        reportError(std::string("Compilation error: ") + e.what());
        return false;
    }
}

bool BrainfuckCompiler::checkBrackets(const std::string& source) {
    int bracketCount = 0;

    for (char c : source) {
        if (c == '[') {
            bracketCount++;
        } else if (c == ']') {
            bracketCount--;
            if (bracketCount < 0) {
                reportError("Syntax error: Extra right bracket ']'");
                return false;
            }
        }
    }

    if (bracketCount != 0) {
        reportError("Syntax error: Brackets do not match");
        return false;
    }

    return true;
}

void BrainfuckCompiler::createMainFunction() {
    // Create main function: int main()
    FunctionType* mainType = FunctionType::get(Type::getInt32Ty(*m_context), // Return type
                                               false // Not variadic
    );

    m_mainFunction = Function::Create(mainType, Function::ExternalLinkage, "main", m_module.get());

    // Create entry basic block
    BasicBlock* entryBlock = BasicBlock::Create(*m_context, "entry", m_mainFunction);

    m_builder->SetInsertPoint(entryBlock);
}

void BrainfuckCompiler::allocateMemory() {
    // Allocate memory array: int8_t memory[memorySize]
    ArrayType* memoryArrayType = ArrayType::get(Type::getInt8Ty(*m_context), m_memorySize);

    m_memoryArray = m_builder->CreateAlloca(memoryArrayType, nullptr, "memory");

    // Initialize memory to 0
    Value* zero = ConstantInt::get(Type::getInt8Ty(*m_context), 0);

    // Use memset to initialize memory
    Function* memsetFunc = Intrinsic::getOrInsertDeclaration(m_module.get(), Intrinsic::memset,
                                                             {m_memoryArray->getType(), Type::getInt8Ty(*m_context)});

    Value* size = ConstantInt::get(Type::getInt64Ty(*m_context), m_memorySize);
    Value* volatileFlag = ConstantInt::get(Type::getInt1Ty(*m_context), false);

    m_builder->CreateCall(
        memsetFunc, {m_builder->CreateBitCast(m_memoryArray, Type::getInt8Ty(*m_context)), zero, size, volatileFlag});

    // Allocate data pointer: int8_t* dataPtr = &memory[memorySize/2]
    m_dataPtr = m_builder->CreateAlloca(Type::getInt8Ty(*m_context), nullptr, "dataptr");

    // Initialize pointer to middle of memory
    Value* indices[] = {ConstantInt::get(Type::getInt32Ty(*m_context), 0),
                        ConstantInt::get(Type::getInt32Ty(*m_context), m_memorySize / 2)};

    Value* initialPtr = m_builder->CreateInBoundsGEP(m_builder->getInt8Ty(), m_memoryArray, indices, "initial_ptr");

    m_builder->CreateStore(initialPtr, m_dataPtr);
}

void BrainfuckCompiler::setupRuntimeFunctions() {
    // Create putchar function declaration: int putchar(int)
    FunctionType* putcharType = FunctionType::get(Type::getInt32Ty(*m_context), {Type::getInt32Ty(*m_context)}, false);

    m_putcharFunc = Function::Create(putcharType, Function::ExternalLinkage, "putchar", m_module.get());

    // Create getchar function declaration: int getchar()
    FunctionType* getcharType = FunctionType::get(Type::getInt32Ty(*m_context), false);

    m_getcharFunc = Function::Create(getcharType, Function::ExternalLinkage, "getchar", m_module.get());
}

void BrainfuckCompiler::generateIR(const std::string& source) {
    m_currentIP = 0;

    // Iterate through each character in source code
    for (size_t i = 0; i < source.length(); ++i) {
        char c = source[i];
        m_currentIP = i;

        // Skip non-Brainfuck instruction characters
        if (c != '>' && c != '<' && c != '+' && c != '-' && c != '.' && c != ',' && c != '[' && c != ']') {
            continue;
        }

        // Count instruction usage
        m_statistics[c]++;

        switch (c) {
        case '>':
            handleIncrementPtr();
            break;
        case '<':
            handleDecrementPtr();
            break;
        case '+':
            handleIncrementByte();
            break;
        case '-':
            handleDecrementByte();
            break;
        case '.':
            handleOutput();
            break;
        case ',':
            handleInput();
            break;
        case '[':
            handleLoopStart(i);
            break;
        case ']':
            handleLoopEnd(i);
            break;
        }
    }

    // Create return instruction
    Value* retValue = ConstantInt::get(Type::getInt32Ty(*m_context), 0);
    m_builder->CreateRet(retValue);
}

void BrainfuckCompiler::handleIncrementPtr() {
    // Load current pointer value
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Pointer increment
    Value* newPtr = m_builder->CreateConstGEP1_32(Type::getInt8Ty(*m_context), currentPtr, 1, "ptr_inc");

    // Store new pointer value
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleDecrementPtr() {
    // Load current pointer value
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Pointer decrement
    Value* newPtr = m_builder->CreateConstGEP1_32(Type::getInt8Ty(*m_context), currentPtr, -1, "ptr_dec");

    // Store new pointer value
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleIncrementByte() {
    // Load current pointer
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Load current byte value
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "current_val");

    // Byte value increment (8-bit unsigned addition)
    Value* newValue = m_builder->CreateAdd(currentValue, ConstantInt::get(Type::getInt8Ty(*m_context), 1), "val_inc",
                                           true, // Signed overflow
                                           true // Unsigned overflow
    );

    // Store new byte value
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleDecrementByte() {
    // Load current pointer
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Load current byte value
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "current_val");

    // Byte value decrement (8-bit unsigned subtraction)
    Value* newValue = m_builder->CreateSub(currentValue, ConstantInt::get(Type::getInt8Ty(*m_context), 1), "val_dec",
                                           true, // Signed overflow
                                           true // Unsigned overflow
    );

    // Store new byte value
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleOutput() {
    // Load current pointer
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Load current byte value
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "output_val");

    // Zero extend to 32-bit (putchar needs int parameter)
    Value* extendedValue = m_builder->CreateZExt(currentValue, Type::getInt32Ty(*m_context), "output_int");

    // Call putchar
    m_builder->CreateCall(m_putcharFunc, {extendedValue});
}

void BrainfuckCompiler::handleInput() {
    // Call getchar
    Value* inputValue = m_builder->CreateCall(m_getcharFunc, {}, "input_char");

    // Truncate to 8-bit
    Value* truncatedValue = m_builder->CreateTrunc(inputValue, Type::getInt8Ty(*m_context), "input_byte");

    // Load current pointer
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");

    // Store input value
    m_builder->CreateStore(truncatedValue, currentPtr);
}

void BrainfuckCompiler::handleLoopStart(size_t ip) {
    // Create loop basic blocks
    BasicBlock* loopHeader = BasicBlock::Create(*m_context, "loop_header_" + std::to_string(ip), m_mainFunction);

    BasicBlock* loopBody = BasicBlock::Create(*m_context, "loop_body_" + std::to_string(ip), m_mainFunction);

    BasicBlock* loopEnd = BasicBlock::Create(*m_context, "loop_end_" + std::to_string(ip), m_mainFunction);

    // Jump to loop header
    m_builder->CreateBr(loopHeader);

    // Set insert point to loop header
    m_builder->SetInsertPoint(loopHeader);

    // Load current byte value
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "loop_val");

    // Compare value to 0
    Value* zero = ConstantInt::get(Type::getInt8Ty(*m_context), 0);
    Value* condition = m_builder->CreateICmpEQ(currentValue, zero, "loop_cond");

    // Conditional branch
    m_builder->CreateCondBr(condition, loopEnd, loopBody);

    // Set insert point to loop body
    m_builder->SetInsertPoint(loopBody);

    // Push to loop stack
    m_loopStartBlocks.push(loopHeader);
    m_loopEndBlocks.push(loopEnd);
}

void BrainfuckCompiler::handleLoopEnd(size_t ip) {
    if (m_loopStartBlocks.empty() || m_loopEndBlocks.empty()) {
        reportError("Syntax error: Extra right bracket ']' at position " + std::to_string(ip));
        return;
    }

    // Get loop basic blocks
    BasicBlock* loopHeader = m_loopStartBlocks.top();
    BasicBlock* loopEnd = m_loopEndBlocks.top();

    // Pop from loop stack
    m_loopStartBlocks.pop();
    m_loopEndBlocks.pop();

    // Jump back to loop header
    m_builder->CreateBr(loopHeader);

    // Set insert point to loop end block
    m_builder->SetInsertPoint(loopEnd);
}

void BrainfuckCompiler::optimizeModule() {
    // Create optimization pass manager
    legacy::PassManager pm;

    // Add optimization passes
    pm.add(createInstructionCombiningPass());
    pm.add(createReassociatePass());
    pm.add(createGVNPass());
    pm.add(createCFGSimplificationPass());

    // Run optimization
    pm.run(*m_module);
}

void BrainfuckCompiler::emitObjectFile(const std::string& outputFile) {
    // Get target machine
    std::string error;
    auto target = TargetRegistry::lookupTarget(m_module->getTargetTriple(), error);

    if (!target) {
        reportError("Target lookup failed: " + error);
        return;
    }

    // Target machine options
    TargetOptions options;
    auto targetMachine =
        target->createTargetMachine(m_module->getTargetTriple(), "generic", "", options, std::optional<Reloc::Model>());

    // Set data layout
    m_module->setDataLayout(targetMachine->createDataLayout());

    // Output filenames
    std::string objectFile = outputFile + ".o";
    std::string executableFile = outputFile;

    // 生成目标文件
    std::error_code ec;
    raw_fd_ostream dest(objectFile, ec, sys::fs::OF_None);

    if (ec) {
        reportError("Cannot open output file: " + ec.message());
        return;
    }

    // Create pass manager
    legacy::PassManager pass;

    // Add object file generation pass
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        reportError("Target machine does not support object file generation");
        return;
    }

    // Run pass
    pass.run(*m_module);
    dest.flush();

    // Link to generate executable file
    std::string linkCommand = "clang " + objectFile + " -o " + executableFile;
    int result = system(linkCommand.c_str());

    if (result != 0) {
        reportError("Linking failed");
        return;
    }

    // Delete object file
    std::remove(objectFile.c_str());

    std::cout << "Compilation completed: " << executableFile << std::endl;
}

void BrainfuckCompiler::executeJIT() {
    // Create JIT execution engine
    std::string error;
    ExecutionEngine* ee = EngineBuilder(std::move(m_module)).setErrorStr(&error).create();

    if (!ee) {
        reportError("JIT engine creation failed: " + error);
        return;
    }

    // Execute main function
    std::vector<GenericValue> noargs;
    GenericValue result = ee->runFunction(m_mainFunction, noargs);

    std::cout << "JIT execution completed, return value: " << result.IntVal.getZExtValue() << std::endl;

    delete ee;
}

void BrainfuckCompiler::createDebugInfo() {
    // Create debug information builder
    m_diBuilder = std::make_unique<DIBuilder>(*m_module);

    // Create compilation unit
    DIFile* file = m_diBuilder->createFile("brainfuck.bf", "/tmp");

    // Create subroutine (main function)
    DISubprogram* sp =
        m_diBuilder->createFunction(file, "main", StringRef(), file, 1,
                                    m_diBuilder->createSubroutineType(m_diBuilder->getOrCreateTypeArray({})), false);

    m_mainFunction->setSubprogram(sp);
}

void BrainfuckCompiler::finalizeDebugInfo() {
    if (m_diBuilder) {
        m_diBuilder->finalize();
    }
}

void BrainfuckCompiler::reportError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}
