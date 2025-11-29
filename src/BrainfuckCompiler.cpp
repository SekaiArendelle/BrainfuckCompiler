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
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // Create LLVM context and module
    m_context = std::make_unique<llvm::LLVMContext>();
    m_module = std::make_unique<llvm::Module>("brainfuck_module", *m_context);
    m_builder = std::make_unique<llvm::IRBuilder<>>(*m_context);

    // Set target triple
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    m_module->setTargetTriple(llvm::Triple(targetTriple));
}

bool BrainfuckCompiler::compile(std::string_view source, std::string_view outputFile, bool enableJIT) {
    try {
        // Check bracket matching
        if (!checkBrackets(source)) {
            return false;
        }

        // Reset statistics
        m_statistics.clear();

        // Create main function and allocate memory
        createMainFunction();
        allocateMemory();
        setupRuntimeFunctions();

        // Generate debug info (if enabled)
        if (m_enableDebugInfo) {
            createDebugInfo();
        }

        // Generate IR
        generateIR(source);

        // Verify IR
        if (llvm::verifyModule(*m_module, &llvm::errs())) {
            reportError("Generated IR is invalid");
            return false;
        }

        // Apply optimizations
        if (m_enableOptimization) {
            optimizeModule();
        }

        // Output IR (for debugging)
        // m_module->print(errs(), nullptr);

        if (enableJIT) {
            // JIT mode: direct execution
            executeJIT();
        } else {
            // Generate object file
            emitObjectFile(outputFile);
        }

        return true;

    } catch (const std::exception& e) {
        reportError(std::string("Compilation error: ") + e.what());
        return false;
    }
}

bool BrainfuckCompiler::checkBrackets(std::string_view source) {
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
    llvm::FunctionType* mainType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*m_context), // Return type
                                                           false // Not variadic
    );

    m_mainFunction = llvm::Function::Create(mainType, llvm::Function::ExternalLinkage, "main", m_module.get());

    // Create entry basic block
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*m_context, "entry", m_mainFunction);

    m_builder->SetInsertPoint(entryBlock);
}

void BrainfuckCompiler::allocateMemory() {
    // Allocate memory array: int8_t memory[memorySize]
    llvm::ArrayType* memoryArrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*m_context), m_memorySize);

    m_memoryArray = m_builder->CreateAlloca(memoryArrayType, nullptr, "memory");

    // Initialize memory to 0 using memset
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*m_context), 0);
    llvm::Value* size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*m_context), m_memorySize);
    llvm::Value* volatileFlag = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*m_context), false);

    // Get the memset intrinsic with correct types
    llvm::Function* memsetFunc = llvm::Intrinsic::getOrInsertDeclaration(
        m_module.get(), llvm::Intrinsic::memset,
        {llvm::PointerType::get(*m_context, 0), llvm::Type::getInt8Ty(*m_context), llvm::Type::getInt64Ty(*m_context)});

    // Create a pointer to the first element of the array
    llvm::Value* firstElementPtr = m_builder->CreatePointerCast(m_memoryArray, llvm::PointerType::get(*m_context, 0));

    m_builder->CreateCall(memsetFunc, {firstElementPtr, zero, size, volatileFlag});

    // Allocate data pointer: int8_t* dataPtr = &memory[memorySize/2]
    m_dataPtr = m_builder->CreateAlloca(llvm::PointerType::get(*m_context, 0), nullptr, "dataptr");

    // Initialize pointer to middle of memory
    llvm::Value* indices[] = {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0),
                              llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), m_memorySize / 2)};

    llvm::Value* initialPtr = m_builder->CreateInBoundsGEP(memoryArrayType, m_memoryArray, indices, "initial_ptr");

    m_builder->CreateStore(initialPtr, m_dataPtr);
}

void BrainfuckCompiler::setupRuntimeFunctions() {
    // Create putchar function declaration: int putchar(int)
    llvm::FunctionType* putcharType =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*m_context), {llvm::Type::getInt32Ty(*m_context)}, false);

    m_putcharFunc = llvm::Function::Create(putcharType, llvm::Function::ExternalLinkage, "putchar", m_module.get());

    // Create getchar function declaration: int getchar()
    llvm::FunctionType* getcharType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*m_context), false);

    m_getcharFunc = llvm::Function::Create(getcharType, llvm::Function::ExternalLinkage, "getchar", m_module.get());
}

void BrainfuckCompiler::generateIR(std::string_view source) {
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
    llvm::Value* retValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0);
    m_builder->CreateRet(retValue);
}

void BrainfuckCompiler::handleIncrementPtr() {
    // Load current pointer value
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Pointer increment
    llvm::Value* newPtr = m_builder->CreateConstGEP1_32(llvm::Type::getInt8Ty(*m_context), currentPtr, 1, "ptr_inc");

    // Store new pointer value
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleDecrementPtr() {
    // Load current pointer value
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Pointer decrement
    llvm::Value* newPtr = m_builder->CreateConstGEP1_32(llvm::Type::getInt8Ty(*m_context), currentPtr, -1, "ptr_dec");

    // Store new pointer value
    m_builder->CreateStore(newPtr, m_dataPtr);
}

void BrainfuckCompiler::handleIncrementByte() {
    // Load current pointer
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Load current byte value
    llvm::Value* currentValue = m_builder->CreateLoad(llvm::Type::getInt8Ty(*m_context), currentPtr, "current_val");

    // Byte value increment (8-bit unsigned addition)
    llvm::Value* newValue =
        m_builder->CreateAdd(currentValue, llvm::ConstantInt::get(llvm::Type::getInt8Ty(*m_context), 1), "val_inc",
                             true, // Signed overflow
                             true // Unsigned overflow
        );

    // Store new byte value
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleDecrementByte() {
    // Load current pointer
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Load current byte value
    llvm::Value* currentValue = m_builder->CreateLoad(llvm::Type::getInt8Ty(*m_context), currentPtr, "current_val");

    // Byte value decrement (8-bit unsigned subtraction)
    llvm::Value* newValue =
        m_builder->CreateSub(currentValue, llvm::ConstantInt::get(llvm::Type::getInt8Ty(*m_context), 1), "val_dec",
                             true, // Signed overflow
                             true // Unsigned overflow
        );

    // Store new byte value
    m_builder->CreateStore(newValue, currentPtr);
}

void BrainfuckCompiler::handleOutput() {
    // Load current pointer
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Load current byte value
    llvm::Value* currentValue = m_builder->CreateLoad(llvm::Type::getInt8Ty(*m_context), currentPtr, "output_val");

    // Zero extend to 32-bit (putchar needs int parameter)
    llvm::Value* extendedValue = m_builder->CreateZExt(currentValue, llvm::Type::getInt32Ty(*m_context), "output_int");

    // Call putchar
    m_builder->CreateCall(m_putcharFunc, {extendedValue});
}

void BrainfuckCompiler::handleInput() {
    // Call getchar
    llvm::Value* inputValue = m_builder->CreateCall(m_getcharFunc, {}, "input_char");

    // Truncate to 8-bit
    llvm::Value* truncatedValue = m_builder->CreateTrunc(inputValue, llvm::Type::getInt8Ty(*m_context), "input_byte");

    // Load current pointer
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");

    // Store input value
    m_builder->CreateStore(truncatedValue, currentPtr);
}

void BrainfuckCompiler::handleLoopStart(size_t ip) {
    // Create loop basic blocks
    llvm::BasicBlock* loopHeader =
        llvm::BasicBlock::Create(*m_context, "loop_header_" + std::to_string(ip), m_mainFunction);

    llvm::BasicBlock* loopBody =
        llvm::BasicBlock::Create(*m_context, "loop_body_" + std::to_string(ip), m_mainFunction);

    llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(*m_context, "loop_end_" + std::to_string(ip), m_mainFunction);

    // Jump to loop header
    m_builder->CreateBr(loopHeader);

    // Set insert point to loop header
    m_builder->SetInsertPoint(loopHeader);

    // Load current byte value
    llvm::Value* currentPtr = m_builder->CreateLoad(llvm::PointerType::get(*m_context, 0), m_dataPtr, "current_ptr");
    llvm::Value* currentValue = m_builder->CreateLoad(llvm::Type::getInt8Ty(*m_context), currentPtr, "loop_val");

    // Compare value to 0
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*m_context), 0);
    llvm::Value* condition = m_builder->CreateICmpEQ(currentValue, zero, "loop_cond");

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
    llvm::BasicBlock* loopHeader = m_loopStartBlocks.top();
    llvm::BasicBlock* loopEnd = m_loopEndBlocks.top();

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
    llvm::legacy::PassManager pm;

    // Add optimization passes
    pm.add(llvm::createInstructionCombiningPass());
    pm.add(llvm::createReassociatePass());
    pm.add(llvm::createGVNPass());
    pm.add(llvm::createCFGSimplificationPass());

    // Run optimization
    pm.run(*m_module);
}

void BrainfuckCompiler::emitObjectFile(std::string_view outputFile) {
    // Get target machine
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(m_module->getTargetTriple(), error);

    if (!target) {
        reportError("Target lookup failed: " + error);
        return;
    }

    // Target machine options
    llvm::TargetOptions options;
    auto targetMachine = target->createTargetMachine(m_module->getTargetTriple(), "generic", "", options,
                                                     std::optional<llvm::Reloc::Model>());

    // Set data layout
    m_module->setDataLayout(targetMachine->createDataLayout());

    // Output filenames
    std::string objectFile = std::string(outputFile) + ".o";
    std::string executableFile = std::string(outputFile);

    // Generate object file
    std::error_code ec;
    llvm::raw_fd_ostream dest(objectFile, ec, llvm::sys::fs::OF_None);

    if (ec) {
        reportError("Cannot open output file: " + ec.message());
        return;
    }

    // Create pass manager
    llvm::legacy::PassManager pass;

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
    llvm::ExecutionEngine* ee = llvm::EngineBuilder(std::move(m_module)).setErrorStr(&error).create();

    if (!ee) {
        reportError("JIT engine creation failed: " + error);
        return;
    }

    // Execute main function
    std::vector<llvm::GenericValue> noargs;
    llvm::GenericValue result = ee->runFunction(m_mainFunction, noargs);

    std::cout << "JIT execution completed, return value: " << result.IntVal.getZExtValue() << std::endl;

    delete ee;
}

void BrainfuckCompiler::createDebugInfo() {
    // Create debug information builder
    m_diBuilder = std::make_unique<llvm::DIBuilder>(*m_module);

    // Create compilation unit
    llvm::DIFile* file = m_diBuilder->createFile("brainfuck.bf", "/tmp");

    // Create subroutine (main function)
    llvm::DISubprogram* sp =
        m_diBuilder->createFunction(file, "main", llvm::StringRef(), file, 1,
                                    m_diBuilder->createSubroutineType(m_diBuilder->getOrCreateTypeArray({})), false);

    m_mainFunction->setSubprogram(sp);
}

void BrainfuckCompiler::finalizeDebugInfo() {
    if (m_diBuilder) {
        m_diBuilder->finalize();
    }
}

void BrainfuckCompiler::reportError(std::string_view message) {
    std::cerr << "Error: " << std::string(message) << std::endl;
}
