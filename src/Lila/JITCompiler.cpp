#include "JITCompiler.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <sstream>

namespace Lila {

JITCompiler::JITCompiler()
{
    std::vector<std::string> possible_include_paths = {
        "/usr/local/include",
        "/usr/include",
        "./install/include",
    };

    std::vector<std::string> possible_lib_paths = {
        "/usr/local/lib",
        "/usr/lib",
        "./install/lib",
    };

    for (const auto& path : possible_include_paths) {
        if (std::filesystem::exists(path)) {
            m_include_paths.push_back(path);
        }
    }

    for (const auto& path : possible_lib_paths) {
        if (std::filesystem::exists(path)) {
            m_library_paths.push_back(path);
        }
    }

    detect_system_includes();
}

void JITCompiler::detect_system_includes()
{
    FILE* pipe = popen("echo | g++ -x c++ -E -Wp,-v - 2>&1 | grep '^ /'", "r");
    if (!pipe) {
        std::cerr << "Warning: Could not detect system includes\n";
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string path(buffer);
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);

        if (!path.empty() && std::filesystem::exists(path)) {
            m_system_include_paths.push_back(path);
            std::cout << "Found system include: " << path << "\n";
        }
    }
    pclose(pipe);
}

JITCompiler::~JITCompiler() = default;

bool JITCompiler::initialize()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jit_or_err = llvm::orc::LLJITBuilder().create();
    if (!jit_or_err) {
        m_last_error = "Failed to create LLJIT engine";
        return false;
    }

    m_jit = std::move(*jit_or_err);

    auto& main_dylib = m_jit->getMainJITDylib();

    auto process_symbols = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
        m_jit->getDataLayout().getGlobalPrefix());

    if (!process_symbols) {
        m_last_error = "Failed to load process symbols";
        return false;
    }

    main_dylib.addGenerator(std::move(*process_symbols));
    std::cout << "Loaded process symbols (includes libstdc++, libc, etc.)\n";

    if (!load_library("libMayaFluxLib.so")) {
        std::cerr << "Warning: Failed to load MayaFlux library: "
                  << m_last_error << "\n";
    }

    return true;
}

bool JITCompiler::load_library(const std::string& library_name)
{
    if (!m_jit) {
        m_last_error = "JIT not initialized";
        return false;
    }

    std::string library_path;
    bool found = false;

    for (const auto& lib_path : m_library_paths) {
        std::string full_path = lib_path + "/" + library_name;
        if (std::filesystem::exists(full_path)) {
            library_path = full_path;
            found = true;
            break;
        }
    }

    if (!found) {
        m_last_error = "Library not found: " + library_name;
        return false;
    }

    std::cout << "Loading library: " << library_path << "\n";

    auto& main_dylib = m_jit->getMainJITDylib();

    auto generator = llvm::orc::DynamicLibrarySearchGenerator::Load(
        library_path.c_str(),
        m_jit->getDataLayout().getGlobalPrefix());

    if (!generator) {
        m_last_error = "Failed to create library search generator: " + llvm::toString(generator.takeError());
        return false;
    }

    main_dylib.addGenerator(std::move(*generator));

    m_loaded_libraries.push_back(library_path);
    std::cout << "Successfully loaded: " << library_path << "\n";

    return true;
}

std::string JITCompiler::generate_unique_name(const std::string& base_name)
{
    int version = m_version_counter.fetch_add(1);
    return base_name + "_v" + std::to_string(version);
}

std::string JITCompiler::wrap_user_code(const std::string& user_code,
    const std::string& unique_entry_point)
{
    std::vector<std::string> includes;
    std::string remaining_code;

    std::istringstream iss(user_code);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));

        if (trimmed.substr(0, 8) == "#include") {
            includes.push_back(line);
        } else {
            remaining_code += line + "\n";
        }
    }

    std::string wrapped;

    wrapped += "#include <algorithm>\n";
    wrapped += "#include <any>\n";
    wrapped += "#include <atomic>\n";
    wrapped += "#include <deque>\n";
    wrapped += "#include <exception>\n";
    wrapped += "#include <functional>\n";
    wrapped += "#include <iostream>\n";
    wrapped += "#include <list>\n";
    wrapped += "#include <map>\n";
    wrapped += "#include <memory>\n";
    wrapped += "#include <mutex>\n";
    wrapped += "#include <numbers>\n";
    wrapped += "#include <numeric>\n";
    wrapped += "#include <optional>\n";
    wrapped += "#include <ranges>\n";
    wrapped += "#include <shared_mutex>\n";
    wrapped += "#include <span>\n";
    wrapped += "#include <string>\n";
    wrapped += "#include <thread>\n";
    wrapped += "#include <unordered_map>\n";
    wrapped += "#include <unordered_set>\n";
    wrapped += "#include <utility>\n";
    wrapped += "#include <variant>\n";
    wrapped += "#include <vector>\n";
    wrapped += "#include <cassert>\n";
    wrapped += "#include <cmath>\n";
    wrapped += "#include <complex>\n";
    wrapped += "#include <cstdint>\n";
    wrapped += "#include <cstring>\n";
    wrapped += "\n";

    for (const auto& inc : includes) {
        wrapped += inc + "\n";
    }
    wrapped += "\n";

    wrapped += "extern \"C\" void " + unique_entry_point + "() {\n";
    wrapped += remaining_code;
    wrapped += "\n}\n";

    return wrapped;
}

bool JITCompiler::compile_to_ir(const std::string& cpp_code,
    const std::string& output_ir_path)
{
    std::string cpp_path = "/tmp/lila_temp.cpp";
    std::ofstream cpp_file(cpp_path);
    if (!cpp_file) {
        m_last_error = "Failed to write temp C++ file";
        return false;
    }
    cpp_file << cpp_code;
    cpp_file.close();

    std::string cmd = "clang++ -S -emit-llvm -O2 -std=c++20 ";

    for (const auto& inc : m_system_include_paths) {
        cmd += "-isystem " + inc + " ";
    }

    for (const auto& inc : m_include_paths) {
        cmd += "-I" + inc + " ";
    }

    cmd += cpp_path + " -o " + output_ir_path + " 2>&1";

    int result = std::system(cmd.c_str());
    if (result != 0) {
        m_last_error = "Clang compilation failed (exit code: " + std::to_string(result) + ")";
        return false;
    }

    return true;
}

bool JITCompiler::compile_and_execute(const std::string& cpp_source,
    const std::string& entry_point)
{
    if (!m_jit) {
        m_last_error = "JIT not initialized";
        return false;
    }

    std::string unique_entry = generate_unique_name(entry_point);
    std::string wrapped_code = wrap_user_code(cpp_source, unique_entry);

    std::string ir_path = "/tmp/lila_" + unique_entry + ".ll";
    if (!compile_to_ir(wrapped_code, ir_path)) {
        return false;
    }

    llvm::LLVMContext ctx;
    llvm::SMDiagnostic err;
    auto module = llvm::parseIRFile(ir_path, err, ctx);

    if (!module) {
        m_last_error = "Failed to parse IR: " + err.getMessage().str();
        return false;
    }

    auto tsm = llvm::orc::ThreadSafeModule(
        std::move(module),
        std::make_unique<llvm::LLVMContext>());

    if (auto add_err = m_jit->addIRModule(std::move(tsm))) {
        m_last_error = "Failed to add module to JIT";
        llvm::logAllUnhandledErrors(std::move(add_err), llvm::errs(), "");
        return false;
    }

    auto sym = m_jit->lookup(unique_entry);
    if (!sym) {
        m_last_error = "Failed to lookup symbol: " + unique_entry;
        llvm::logAllUnhandledErrors(sym.takeError(), llvm::errs(), "");
        return false;
    }

    auto* func = sym->toPtr<void (*)()>();
    func();

    return true;
}

} // namespace Lila
