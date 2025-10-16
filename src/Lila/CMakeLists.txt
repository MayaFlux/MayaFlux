add_definitions(${CLANG_DEFINITIONS})

set(LILA_SOURCES
    Lila.cpp
    ClangInterpreter.cpp
    EventBus.cpp
    Server.cpp
)

set(LILA_HEADERS
    ClangInterpreter.hpp
    Commentator.hpp
    EventBus.hpp
    LiveAid.hpp
    Lila.hpp
    Server.hpp
)

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/lila_config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LilaConfig.hpp"
    @ONLY
)

add_library(Lila SHARED ${LILA_SOURCES} ${LILA_HEADERS})

target_include_directories(Lila PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For #include "Lila/..."
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_precompile_headers(Lila PUBLIC ../../cmake/pch.h)
target_include_directories(Lila PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(Lila PUBLIC ${LLVM_DEFINITIONS_LIST})

set_target_properties(Lila PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(UNIX)
    execute_process(
        COMMAND llvm-config --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --libs core orcjit native irreader support
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    target_link_libraries(Lila PUBLIC
        ${LLVM_LDFLAGS}
        ${LLVM_LIBS}
        ${LLVM_SYSTEM_LIBS}
    )
    target_link_libraries(Lila PRIVATE clang-cpp)
else()
    set(LLVM_LIB_DIR "C:/Program Files/LLVM/lib")
    
    set(LLVM_LIBS
        "${LLVM_LIB_DIR}/LLVMCore.lib"
        "${LLVM_LIB_DIR}/LLVMSupport.lib" 
        "${LLVM_LIB_DIR}/LLVMOrcJIT.lib"
        "${LLVM_LIB_DIR}/LLVMIRReader.lib"
        "${LLVM_LIB_DIR}/LLVMExecutionEngine.lib"
        "${LLVM_LIB_DIR}/LLVMRuntimeDyld.lib"
        "${LLVM_LIB_DIR}/LLVMX86CodeGen.lib"
        "${LLVM_LIB_DIR}/LLVMX86AsmParser.lib"
        "${LLVM_LIB_DIR}/LLVMX86Desc.lib"
        "${LLVM_LIB_DIR}/LLVMX86Info.lib"
        "${LLVM_LIB_DIR}/LLVMTarget.lib"
        "${LLVM_LIB_DIR}/LLVMMC.lib"
        "${LLVM_LIB_DIR}/LLVMMCParser.lib"
        "${LLVM_LIB_DIR}/LLVMObject.lib"
        "${LLVM_LIB_DIR}/LLVMTransformUtils.lib"
        "${LLVM_LIB_DIR}/LLVMAnalysis.lib"
        "${LLVM_LIB_DIR}/LLVMPasses.lib"
        "${LLVM_LIB_DIR}/LLVMBitReader.lib"
        "${LLVM_LIB_DIR}/LLVMAsmParser.lib"
        "${LLVM_LIB_DIR}/LLVMCodeGen.lib"
        "${LLVM_LIB_DIR}/LLVMSelectionDAG.lib"
        "${LLVM_LIB_DIR}/LLVMAsmPrinter.lib"
        "${LLVM_LIB_DIR}/LLVMGlobalISel.lib"
        "${LLVM_LIB_DIR}/LLVMMIRParser.lib"
        "${LLVM_LIB_DIR}/LLVMProfileData.lib"
        # ADD THESE MISSING ONES:
        "${LLVM_LIB_DIR}/LLVMBinaryFormat.lib"
        "${LLVM_LIB_DIR}/LLVMDemangle.lib"
        "${LLVM_LIB_DIR}/LLVMRemarks.lib"
        "${LLVM_LIB_DIR}/LLVMBitstreamReader.lib"
    )
    
    set(CLANG_LIBS
        "${LLVM_LIB_DIR}/clangInterpreter.lib"
        "${LLVM_LIB_DIR}/clangFrontend.lib"
        "${LLVM_LIB_DIR}/clangDriver.lib"
        "${LLVM_LIB_DIR}/clangSerialization.lib"
        "${LLVM_LIB_DIR}/clangCodeGen.lib"
        "${LLVM_LIB_DIR}/clangParse.lib"
        "${LLVM_LIB_DIR}/clangSema.lib"
        "${LLVM_LIB_DIR}/clangAnalysis.lib"
        "${LLVM_LIB_DIR}/clangEdit.lib"
        "${LLVM_LIB_DIR}/clangAST.lib"
        "${LLVM_LIB_DIR}/clangLex.lib"
        "${LLVM_LIB_DIR}/clangBasic.lib"
        # ADD THESE MISSING ONES:
        "${LLVM_LIB_DIR}/clangASTMatchers.lib"
        "${LLVM_LIB_DIR}/clangTooling.lib"
        "${LLVM_LIB_DIR}/clangFormat.lib"
    )
    
    target_link_libraries(Lila PRIVATE
        ${LLVM_LIBS}
        ${CLANG_LIBS}
        ws2_32  # Windows sockets
    )
endif()

target_link_libraries(Lila PUBLIC MayaFluxLib)

