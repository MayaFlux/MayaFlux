#pragma once

#include "SymbolRegistry.hpp"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

namespace Lila {

class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    bool initialize();

    bool load_library(const std::string& library_name);

    bool compile_and_execute(const std::string& cpp_source,
        const std::string& entry_point = "live_reload");

    std::string get_last_error() const { return m_last_error; }

    const SymbolRegistry& get_symbol_registry() const { return m_symbol_registry; }

private:
    std::unique_ptr<llvm::orc::LLJIT> m_jit;
    std::atomic<int> m_version_counter { 0 };
    std::string m_last_error;

    std::vector<std::string> m_include_paths;
    std::vector<std::string> m_library_paths;
    std::vector<std::string> m_system_include_paths;

    std::vector<std::string> m_loaded_libraries;

    SymbolRegistry m_symbol_registry;

    bool compile_to_ir(const std::string& cpp_code,
        const std::string& output_ir_path);

    std::string generate_unique_name(const std::string& base_name);

    std::string wrap_user_code(const std::string& user_code,
        const std::string& unique_entry_point);

    void detect_system_includes();

    std::string m_pch_path;
};

} // namespace Lila
