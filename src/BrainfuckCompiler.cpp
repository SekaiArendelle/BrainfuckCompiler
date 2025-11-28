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
    
    // 初始化LLVM
    initializeLLVM();
}

BrainfuckCompiler::~BrainfuckCompiler() {
    // 清理资源
    if (m_diBuilder) {
        finalizeDebugInfo();
    }
}

void BrainfuckCompiler::initializeLLVM() {
    // 初始化LLVM目标
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();
    
    // 创建LLVM上下文和模块
    m_context = std::make_unique<LLVMContext>();
    m_module = std::make_unique<Module>("brainfuck_module", *m_context);
    m_builder = std::make_unique<IRBuilder<>>(*m_context);
    
    // 设置目标三元组
    auto targetTriple = sys::getDefaultTargetTriple();
    m_module->setTargetTriple(llvm::Triple(targetTriple));
}

bool BrainfuckCompiler::compile(const std::string& source, const std::string& outputFile, bool enableJIT) {
    try {
        // 检查括号匹配
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
            reportError("生成的IR无效");
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
        reportError(std::string("编译错误: ") + e.what());
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
                reportError("语法错误: 多余的右括号 ']'");
                return false;
            }
        }
    }
    
    if (bracketCount != 0) {
        reportError("语法错误: 括号不匹配");
        return false;
    }
    
    return true;
}

void BrainfuckCompiler::createMainFunction() {
    // 创建main函数: int main()
    FunctionType* mainType = FunctionType::get(
        Type::getInt32Ty(*m_context),  // 返回类型
        false                           // 不是可变参数
    );
    
    m_mainFunction = Function::Create(
        mainType,
        Function::ExternalLinkage,
        "main",
        m_module.get()
    );
    
    // 创建入口基本块
    BasicBlock* entryBlock = BasicBlock::Create(
        *m_context,
        "entry",
        m_mainFunction
    );
    
    m_builder->SetInsertPoint(entryBlock);
}

void BrainfuckCompiler::allocateMemory() {
    // 分配内存数组: int8_t memory[memorySize]
    ArrayType* memoryArrayType = ArrayType::get(
        Type::getInt8Ty(*m_context),
        m_memorySize
    );
    
    m_memoryArray = m_builder->CreateAlloca(
        memoryArrayType,
        nullptr,
        "memory"
    );
    
    // 将内存初始化为0
    Value* zero = ConstantInt::get(Type::getInt8Ty(*m_context), 0);
    
    // 使用memset初始化内存
    Function* memsetFunc = Intrinsic::getOrInsertDeclaration(
        m_module.get(),
        Intrinsic::memset,
        { m_memoryArray->getType(), Type::getInt8Ty(*m_context) }
    );
    
    Value* size = ConstantInt::get(Type::getInt64Ty(*m_context), m_memorySize);
    Value* volatileFlag = ConstantInt::get(Type::getInt1Ty(*m_context), false);
    
    m_builder->CreateCall(memsetFunc, {
        m_builder->CreateBitCast(m_memoryArray, Type::getInt8Ty(*m_context)),
        zero,
        size,
        volatileFlag
    });
    
    // 分配数据指针: int8_t* dataPtr = &memory[memorySize/2]
    m_dataPtr = m_builder->CreateAlloca(
        Type::getInt8Ty(*m_context),
        nullptr,
        "dataptr"
    );
    
    // 初始化指针到内存中间位置
    Value* indices[] = {
        ConstantInt::get(Type::getInt32Ty(*m_context), 0),
        ConstantInt::get(Type::getInt32Ty(*m_context), m_memorySize / 2)
    };
    
    Value* initialPtr = m_builder->CreateInBoundsGEP(
        m_builder->getInt8Ty(),
        m_memoryArray,
        indices,
        "initial_ptr"
    );
    
    m_builder->CreateStore(initialPtr, m_dataPtr);
}

void BrainfuckCompiler::setupRuntimeFunctions() {
    // 创建putchar函数声明: int putchar(int)
    FunctionType* putcharType = FunctionType::get(
        Type::getInt32Ty(*m_context),
        { Type::getInt32Ty(*m_context) },
        false
    );
    
    m_putcharFunc = Function::Create(
        putcharType,
        Function::ExternalLinkage,
        "putchar",
        m_module.get()
    );
    
    // 创建getchar函数声明: int getchar()
    FunctionType* getcharType = FunctionType::get(
        Type::getInt32Ty(*m_context),
        false
    );
    
    m_getcharFunc = Function::Create(
        getcharType,
        Function::ExternalLinkage,
        "getchar",
        m_module.get()
    );
}

void BrainfuckCompiler::generateIR(const std::string& source) {
    m_currentIP = 0;
    
    // 遍历源代码中的每个字符
    for (size_t i = 0; i < source.length(); ++i) {
        char c = source[i];
        m_currentIP = i;
        
        // 跳过非Brainfuck指令字符
        if (c != '>' && c != '<' && c != '+' && c != '-' && 
            c != '.' && c != ',' && c != '[' && c != ']') {
            continue;
        }
        
        // 统计指令使用
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
    
    // 创建返回指令
    Value* retValue = ConstantInt::get(Type::getInt32Ty(*m_context), 0);
    m_builder->CreateRet(retValue);
}

void BrainfuckCompiler::handleIncrementPtr() {
    // 加载当前指针值
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 指针递增
    Value* newPtr = m_builder->CreateConstGEP1_32(
        Type::getInt8Ty(*m_context),
        currentPtr,
        1,
        "ptr_inc"
    );
    
    // 存储新指针值
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleDecrementPtr() {
    // 加载当前指针值
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 指针递减
    Value* newPtr = m_builder->CreateConstGEP1_32(
        Type::getInt8Ty(*m_context),
        currentPtr,
        -1,
        "ptr_dec"
    );
    
    // 存储新指针值
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleIncrementByte() {
    // 加载当前指针
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 加载当前字节值
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "current_val");
    
    // 字节值递增（8位无符号加法）
    Value* newValue = m_builder->CreateAdd(
        currentValue,
        ConstantInt::get(Type::getInt8Ty(*m_context), 1),
        "val_inc",
        true,  // 有符号溢出
        true   // 无符号溢出
    );
    
    // 存储新字节值
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleDecrementByte() {
    // 加载当前指针
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 加载当前字节值
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "current_val");
    
    // 字节值递减（8位无符号减法）
    Value* newValue = m_builder->CreateSub(
        currentValue,
        ConstantInt::get(Type::getInt8Ty(*m_context), 1),
        "val_dec",
        true,  // 有符号溢出
        true   // 无符号溢出
    );
    
    // 存储新字节值
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleOutput() {
    // 加载当前指针
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 加载当前字节值
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "output_val");
    
    // 零扩展到32位（putchar需要int参数）
    Value* extendedValue = m_builder->CreateZExt(
        currentValue,
        Type::getInt32Ty(*m_context),
        "output_int"
    );
    
    // 调用putchar
    m_builder->CreateCall(m_putcharFunc, { extendedValue });
}

void BrainfuckCompiler::handleInput() {
    // 调用getchar
    Value* inputValue = m_builder->CreateCall(m_getcharFunc, {}, "input_char");
    
    // 截断到8位
    Value* truncatedValue = m_builder->CreateTrunc(
        inputValue,
        Type::getInt8Ty(*m_context),
        "input_byte"
    );
    
    // 加载当前指针
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    
    // 存储输入值
    m_builder->CreateStore(truncatedValue, currentPtr);
}

void BrainfuckCompiler::handleLoopStart(size_t ip) {
    // 创建循环基本块
    BasicBlock* loopHeader = BasicBlock::Create(
        *m_context,
        "loop_header_" + std::to_string(ip),
        m_mainFunction
    );
    
    BasicBlock* loopBody = BasicBlock::Create(
        *m_context,
        "loop_body_" + std::to_string(ip),
        m_mainFunction
    );
    
    BasicBlock* loopEnd = BasicBlock::Create(
        *m_context,
        "loop_end_" + std::to_string(ip),
        m_mainFunction
    );
    
    // 跳转到循环头部
    m_builder->CreateBr(loopHeader);
    
    // 设置插入点到循环头部
    m_builder->SetInsertPoint(loopHeader);
    
    // 加载当前字节值
    Value* currentPtr = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), m_dataPtr, "current_ptr");
    Value* currentValue = m_builder->CreateLoad(Type::getInt8Ty(m_builder->getContext()), currentPtr, "loop_val");
    
    // 比较值是否为0
    Value* zero = ConstantInt::get(Type::getInt8Ty(*m_context), 0);
    Value* condition = m_builder->CreateICmpEQ(currentValue, zero, "loop_cond");
    
    // 条件分支
    m_builder->CreateCondBr(condition, loopEnd, loopBody);
    
    // 设置插入点到循环体
    m_builder->SetInsertPoint(loopBody);
    
    // 压入循环栈
    m_loopStartBlocks.push(loopHeader);
    m_loopEndBlocks.push(loopEnd);
}

void BrainfuckCompiler::handleLoopEnd(size_t ip) {
    if (m_loopStartBlocks.empty() || m_loopEndBlocks.empty()) {
        reportError("语法错误: 多余的右括号 ']' 在位置 " + std::to_string(ip));
        return;
    }
    
    // 获取循环基本块
    BasicBlock* loopHeader = m_loopStartBlocks.top();
    BasicBlock* loopEnd = m_loopEndBlocks.top();
    
    // 弹出循环栈
    m_loopStartBlocks.pop();
    m_loopEndBlocks.pop();
    
    // 跳回循环头部
    m_builder->CreateBr(loopHeader);
    
    // 设置插入点到循环结束块
    m_builder->SetInsertPoint(loopEnd);
}

void BrainfuckCompiler::optimizeModule() {
    // 创建优化pass管理器
    legacy::PassManager pm;
    
    // 添加优化passes
    pm.add(createInstructionCombiningPass());
    pm.add(createReassociatePass());
    pm.add(createGVNPass());
    pm.add(createCFGSimplificationPass());
    
    // 运行优化
    pm.run(*m_module);
}

void BrainfuckCompiler::emitObjectFile(const std::string& outputFile) {
    // 获取目标机器
    std::string error;
    auto target = TargetRegistry::lookupTarget(m_module->getTargetTriple(), error);
    
    if (!target) {
        reportError("目标查找失败: " + error);
        return;
    }
    
    // 目标机器选项
    TargetOptions options;
    auto targetMachine = target->createTargetMachine(
        m_module->getTargetTriple(),
        "generic",
        "",
        options,
        std::optional<Reloc::Model>()
    );
    
    // 设置数据布局
    m_module->setDataLayout(targetMachine->createDataLayout());
    
    // 输出文件名
    std::string objectFile = outputFile + ".o";
    std::string executableFile = outputFile;
    
    // 生成目标文件
    std::error_code ec;
    raw_fd_ostream dest(objectFile, ec, sys::fs::OF_None);
    
    if (ec) {
        reportError("无法打开输出文件: " + ec.message());
        return;
    }
    
    // 创建pass管理器
    legacy::PassManager pass;
    
    // 添加目标文件生成pass
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        reportError("目标机器不支持目标文件生成");
        return;
    }
    
    // 运行pass
    pass.run(*m_module);
    dest.flush();
    
    // 链接生成可执行文件
    std::string linkCommand = "clang " + objectFile + " -o " + executableFile;
    int result = system(linkCommand.c_str());
    
    if (result != 0) {
        reportError("链接失败");
        return;
    }
    
    // 删除目标文件
    std::remove(objectFile.c_str());
    
    std::cout << "编译完成: " << executableFile << std::endl;
}

void BrainfuckCompiler::executeJIT() {
    // 创建JIT执行引擎
    std::string error;
    ExecutionEngine* ee = EngineBuilder(std::move(m_module))
        .setErrorStr(&error)
        .create();
    
    if (!ee) {
        reportError("JIT引擎创建失败: " + error);
        return;
    }
    
    // 执行main函数
    std::vector<GenericValue> noargs;
    GenericValue result = ee->runFunction(m_mainFunction, noargs);
    
    std::cout << "JIT执行完成，返回值: " << result.IntVal.getZExtValue() << std::endl;
    
    delete ee;
}

void BrainfuckCompiler::createDebugInfo() {
    // 创建调试信息构建器
    m_diBuilder = std::make_unique<DIBuilder>(*m_module);
    
    // 创建编译单元
    DIFile* file = m_diBuilder->createFile("brainfuck.bf", "/tmp");
    
    // 创建子程序（main函数）
    DISubprogram* sp = m_diBuilder->createFunction(
        file,
        "main",
        StringRef(),
        file,
        1,
        m_diBuilder->createSubroutineType(m_diBuilder->getOrCreateTypeArray({})),
        false
    );
    
    m_mainFunction->setSubprogram(sp);
}

void BrainfuckCompiler::finalizeDebugInfo() {
    if (m_diBuilder) {
        m_diBuilder->finalize();
    }
}

void BrainfuckCompiler::reportError(const std::string& message) {
    std::cerr << "错误: " << message << std::endl;
}
