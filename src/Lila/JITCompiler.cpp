#include "JITCompiler.hpp"

#include "sstream"
#include <filesystem>
#include <fstream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>

namespace Lila {

JITCompiler::JITCompiler()
{
    m_include_paths.push_back("./src");
    m_library_paths.push_back("./build/src/MayaFlux");
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
    std::string code_without_includes;

    std::istringstream iss(user_code);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));

        if (trimmed.substr(0, 8) == "#include") {
            includes.push_back(line);
        } else {
            code_without_includes += line + "\n";
        }
    }

    std::string wrapped;

    for (const auto& inc : includes) {
        wrapped += inc + "\n";
    }
    wrapped += "\n";

    bool has_live_reload = (code_without_includes.find("void live_reload()") != std::string::npos);

    if (has_live_reload) {
        wrapped += code_without_includes;
        wrapped += "\n";
        wrapped += "extern \"C\" void " + unique_entry_point + "() {\n";
        wrapped += "    live_reload();\n";
        wrapped += "}\n";
    } else {
        wrapped += "extern \"C\" void " + unique_entry_point + "() {\n";
        wrapped += code_without_includes;
        wrapped += "}\n";
    }

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

    for (const auto& include : m_include_paths) {
        cmd += "-I" + include + " ";
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
