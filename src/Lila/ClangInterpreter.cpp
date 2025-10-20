#include "ClangInterpreter.hpp"

#include "Commentator.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include <clang/AST/Type.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

namespace Lila {

struct ClangInterpreter::Impl {
    std::unique_ptr<clang::Interpreter> interpreter;

    std::vector<std::string> include_paths;
    std::vector<std::string> compile_flags;
    std::string target_triple;

    std::unordered_map<std::string, void*> symbol_table;
    int eval_counter = 0;
};

ClangInterpreter::ClangInterpreter()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->target_triple = llvm::sys::getDefaultTargetTriple();
}

ClangInterpreter::~ClangInterpreter()
{
    shutdown();
}

ClangInterpreter::ClangInterpreter(ClangInterpreter&&) noexcept = default;
ClangInterpreter& ClangInterpreter::operator=(ClangInterpreter&&) noexcept = default;

bool ClangInterpreter::initialize()
{
    LILA_INFO(Emitter::INTERPRETER, "Initializing Clang interpreter");

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    llvm::sys::DynamicLibrary::LoadLibraryPermanently("msvcp140.dll");
    llvm::sys::DynamicLibrary::LoadLibraryPermanently("vcruntime140.dll");
    llvm::sys::DynamicLibrary::LoadLibraryPermanently("ucrtbase.dll");
    llvm::sys::DynamicLibrary::LoadLibraryPermanently("MayaFluxLib.dll");
#endif

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    m_impl->compile_flags.clear();
    m_impl->compile_flags.emplace_back("-std=c++23");
    m_impl->compile_flags.emplace_back("-DMAYASIMPLE");

    std::string pch_dir;
    if (std::filesystem::exists(MayaFlux::Config::PCH_RUNTIME_PATH)) {
        pch_dir = MayaFlux::Config::RUNTIME_DATA_DIR;
        LILA_DEBUG(Emitter::INTERPRETER,
            std::string("Using installed PCH from: ") + std::string(MayaFlux::Config::PCH_RUNTIME_PATH));

    } else if (std::filesystem::exists(MayaFlux::Config::PCH_SOURCE_PATH)) {
        pch_dir = std::string(MayaFlux::Config::SOURCE_DIR) + "/cmake";
        LILA_DEBUG(Emitter::INTERPRETER,
            std::string("Using source PCH from: ") + std::string(MayaFlux::Config::PCH_SOURCE_PATH));

    } else {
        m_last_error = "Cannot find pch.h in runtime or source locations";
        LILA_ERROR(Emitter::INTERPRETER, m_last_error);
        return false;
    }

    m_impl->compile_flags.push_back("-I" + pch_dir);

    std::string resource_dir = MayaFlux::Platform::SystemConfig::get_clang_resource_dir();
    if (!resource_dir.empty()) {
        m_impl->compile_flags.push_back("-resource-dir=" + resource_dir);
        LILA_DEBUG(Emitter::INTERPRETER,
            std::string("Using clang resource dir: ") + resource_dir);
    } else {
        m_impl->compile_flags.emplace_back("-resource-dir=/usr/lib/clang/20");
        LILA_WARN(Emitter::INTERPRETER,
            "Using default clang resource dir: /usr/lib/clang/20");
    }

    auto system_includes = MayaFlux::Platform::SystemConfig::get_system_includes();
    for (const auto& include : system_includes) {
        m_impl->compile_flags.push_back("-isystem" + include);
    }

    for (const auto& path : m_impl->include_paths) {
        m_impl->compile_flags.push_back("-I" + path);
    }

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    m_impl->compile_flags.emplace_back("-fno-function-sections");
    m_impl->compile_flags.emplace_back("-fno-data-sections");
    m_impl->compile_flags.emplace_back("-fno-unique-section-names");
#endif

    std::vector<const char*> args;
    for (const auto& flag : m_impl->compile_flags) {
        args.push_back(flag.c_str());
    }

    clang::IncrementalCompilerBuilder ICB;
    ICB.SetCompilerArgs(args);

    auto CI = ICB.CreateCpp();
    if (!CI) {
        m_last_error = "Failed to create CompilerInstance: " + llvm::toString(CI.takeError());
        LILA_ERROR(Emitter::INTERPRETER, m_last_error);
        return false;
    }

    auto interp = clang::Interpreter::create(std::move(*CI));
    if (!interp) {
        m_last_error = "Failed to create interpreter: " + llvm::toString(interp.takeError());
        LILA_ERROR(Emitter::INTERPRETER, m_last_error);
        return false;
    }

    m_impl->interpreter = std::move(*interp);

    LILA_INFO(Emitter::INTERPRETER, "Clang interpreter created successfully");

    auto result = m_impl->interpreter->ParseAndExecute(
        "#include \"pch.h\"\n"
        "#include \"Lila/LiveAid.hpp\"\n");

    if (result) {
        std::string warning = "Failed to load MayaFlux headers: " + llvm::toString(std::move(result));
        LILA_WARN(Emitter::INTERPRETER, warning);
    } else {
        LILA_INFO(Emitter::INTERPRETER, "MayaFlux headers loaded successfully");
    }

    result = m_impl->interpreter->ParseAndExecute("std::cout << \"Ready for Live\" << std::flush;");

    return true;
}

ClangInterpreter::EvalResult ClangInterpreter::eval(const std::string& code)
{
    EvalResult result;

    if (!m_impl->interpreter) {
        result.error = "Interpreter not initialized";
        LILA_ERROR(Emitter::INTERPRETER, result.error);
        return result;
    }

    LILA_DEBUG(Emitter::INTERPRETER, "Evaluating code...");

    auto eval_result = m_impl->interpreter->ParseAndExecute(code);

    if (!eval_result) {
        result.success = true;
        LILA_DEBUG(Emitter::INTERPRETER, "Code evaluation succeeded");
    } else {
        result.success = false;
        result.error = "Execution failed: " + llvm::toString(std::move(eval_result));
        LILA_ERROR(Emitter::INTERPRETER, result.error);
    }

    m_impl->eval_counter++;
    return result;
}

ClangInterpreter::EvalResult ClangInterpreter::eval_file(const std::string& filepath)
{
    EvalResult result;

    if (!std::filesystem::exists(filepath)) {
        result.error = "File does not exist: " + filepath;
        LILA_ERROR(Emitter::INTERPRETER, result.error);
        return result;
    }

    LILA_INFO(Emitter::INTERPRETER, std::string("Evaluating file: ") + filepath);

    std::string code = "#include \"" + filepath + "\"\n";
    return eval(code);
}

void* ClangInterpreter::get_symbol_address(const std::string& name)
{
    if (!m_impl->interpreter) {
        LILA_WARN(Emitter::INTERPRETER, "Cannot get symbol: interpreter not initialized");
        return nullptr;
    }

    auto symbol = m_impl->interpreter->getSymbolAddress(name);
    if (symbol) {
        LILA_DEBUG(Emitter::INTERPRETER, std::string("Found symbol: ") + name);
        return reinterpret_cast<void*>(symbol->getValue());
    }

    LILA_WARN(Emitter::INTERPRETER, std::string("Symbol not found: ") + name);
    return nullptr;
}

std::vector<std::string> ClangInterpreter::get_defined_symbols()
{
    std::vector<std::string> symbols;
    for (const auto& pair : m_impl->symbol_table) {
        symbols.push_back(pair.first);
    }
    return symbols;
}

void ClangInterpreter::add_include_path(const std::string& path)
{
    if (std::filesystem::exists(path)) {
        m_impl->include_paths.push_back(path);
        LILA_DEBUG(Emitter::INTERPRETER, std::string("Added include path: ") + path);
    } else {
        LILA_WARN(Emitter::INTERPRETER,
            std::string("Include path does not exist: ") + path);
    }
}

void ClangInterpreter::add_library_path(const std::string& path)
{
    LILA_DEBUG(Emitter::INTERPRETER,
        std::string("Library path noted (not yet implemented): ") + path);
}

void ClangInterpreter::add_compile_flag(const std::string& flag)
{
    m_impl->compile_flags.push_back(flag);
    LILA_DEBUG(Emitter::INTERPRETER, std::string("Added compile flag: ") + flag);
}

void ClangInterpreter::set_target_triple(const std::string& triple)
{
    m_impl->target_triple = triple;
    LILA_INFO(Emitter::INTERPRETER, std::string("Target triple set to: ") + triple);
}

void ClangInterpreter::reset()
{
    LILA_INFO(Emitter::INTERPRETER, "Resetting interpreter");
    shutdown();
    m_impl = std::make_unique<Impl>();
}

void ClangInterpreter::shutdown()
{
    if (m_impl->interpreter) {
        LILA_INFO(Emitter::INTERPRETER, "Shutting down interpreter");
        m_impl->interpreter.reset();
    }
}

} // namespace Lila
