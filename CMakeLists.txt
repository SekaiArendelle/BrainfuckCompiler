cmake_minimum_required(VERSION 3.10)
project(BrainfuckCompiler VERSION 1.0.0 LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 查找LLVM包
find_package(LLVM REQUIRED CONFIG)

# 打印LLVM信息
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 包含LLVM设置
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 源文件
set(SOURCES
    src/BrainfuckCompiler.cpp
    src/main.cpp
)

# 创建可执行文件
add_executable(bfc ${SOURCES})

target_link_libraries(bfc LLVM)

# 设置编译器标志
target_compile_features(bfc PRIVATE cxx_std_17)

# 不同构建类型的编译选项
target_compile_options(bfc PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra -Wpedantic>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -Wall -Wextra>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -Wall -Wextra>
    $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG -Wall -Wextra>
)

# 安装规则
install(TARGETS bfc
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 测试
enable_testing()

# 添加测试
add_test(NAME basic_compilation 
    COMMAND ${CMAKE_BINARY_DIR}/bin/bfc -i ${CMAKE_SOURCE_DIR}/examples/hello.bf -o hello -O
)

# CPack配置
set(CPACK_PACKAGE_NAME "BrainfuckCompiler")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Brainfuck to LLVM Compiler")
set(CPACK_PACKAGE_VENDOR "LLVM Brainfuck Compiler")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libstdc++6, llvm-14")

include(CPack)
