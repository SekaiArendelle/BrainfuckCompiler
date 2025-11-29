#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <memory>
#include "BrainfuckCompiler.h"

/**
 * @brief Display usage help
 */
void showUsage(const char* programName) {
    // Null pointer check with local fallback
    if (!programName) {
        programName = "bf_compiler";
    }

    // Using raw string literal for cleaner formatting
    std::cout << "Brainfuck LLVM Compiler\n"
                 "Usage: "
              << programName
              << " [options]\n\n"
                 "Options:\n"
                 "  -i, --input <file>     Input Brainfuck source file\n"
                 "  -o, --output <file>    Output executable filename\n"
                 "  -m, --memory <size>    Memory size (default: 30000)\n"
                 "  -O, --optimize         Enable optimization\n"
                 "  -g, --debug            Generate debug info\n"
                 "  -j, --jit              JIT mode direct execution\n"
                 "  -s, --stats            Show compilation statistics\n"
                 "  -h, --help             Show help information\n\n"
                 "Examples:\n"
                 "  "
              << programName
              << " -i hello.bf -o hello\n"
                 "  "
              << programName
              << " -i mandelbrot.bf -o mandelbrot -O -m 60000\n"
                 "  "
              << programName << " -i test.bf -j -s\n";
}

/**
 * @brief Read file content
 */
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief Parse command line arguments
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
                throw std::runtime_error("Missing input filename parameter");
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                options.outputFile = argv[++i];
            } else {
                throw std::runtime_error("Missing output filename parameter");
            }
        } else if (arg == "-m" || arg == "--memory") {
            if (i + 1 < argc) {
                options.memorySize = std::stoul(argv[++i]);
            } else {
                throw std::runtime_error("Missing memory size parameter");
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
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    return options;
}

/**
 * @brief Display compilation statistics
 */
void showStatistics(const BrainfuckCompiler& compiler) {
    auto stats = compiler.getStatistics();

    std::cout << "\n=== Compilation Statistics ===" << std::endl;
    std::cout << "Instruction usage statistics:" << std::endl;

    const char* instructionNames[] = {">", "<", "+", "-", ".", ",", "[", "]"};
    const char* instructionDesc[] = {"Pointer right", "Pointer left", "Byte increment", "Byte decrement",
                                     "Output",        "Input",        "Loop start",     "Loop end"};

    size_t totalInstructions = 0;

    for (size_t i = 0; i < 8; ++i) {
        char instr = *instructionNames[i];
        if (stats.count(instr) > 0) {
            std::cout << "  '" << instr << "' (" << instructionDesc[i] << "): " << stats.at(instr) << " times"
                      << std::endl;
            totalInstructions += stats.at(instr);
        }
    }

    std::cout << "Total instructions: " << totalInstructions << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        CommandLineOptions options = parseCommandLine(argc, argv);

        // Display help
        if (options.showHelp) {
            showUsage(argv[0]);
            return 0;
        }

        // Check required parameters
        if (options.inputFile.empty()) {
            std::cerr << "Error: Input file must be specified" << std::endl;
            std::cerr << "Use '" << argv[0] << " --help' for usage" << std::endl;
            return 1;
        }

        // Read source file
        std::string sourceCode = readFile(options.inputFile);

        // Create compiler
        BrainfuckCompiler compiler(options.memorySize);

        // Set compilation options
        compiler.setOptimization(options.enableOptimization);
        compiler.setDebugInfo(options.enableDebugInfo);

        // Compile
        std::cout << "Compiling: " << options.inputFile << std::endl;
        std::cout << "Memory size: " << options.memorySize << " bytes" << std::endl;
        std::cout << "Optimization: " << (options.enableOptimization ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Debug info: " << (options.enableDebugInfo ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Execution mode: " << (options.enableJIT ? "JIT" : "Compile") << std::endl;

        bool success = compiler.compile(sourceCode, options.outputFile, options.enableJIT);

        if (!success) {
            std::cerr << "Compilation failed" << std::endl;
            return 1;
        }

        // Display statistics
        if (options.showStats) {
            showStatistics(compiler);
        }

        std::cout << "Compilation successful!" << std::endl;

        if (!options.enableJIT) {
            std::cout << "Output file: " << options.outputFile << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
