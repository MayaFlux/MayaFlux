#pragma once

namespace clang {
class Interpreter;
class CompilerInstance;
}

namespace Lila {

class ClangInterpreter {
public:
    ClangInterpreter();
    ~ClangInterpreter();

    bool initialize();
    void shutdown();

    struct EvalResult {
        bool success = false;
        std::string output;
        std::string error;
        void* symbol_address = nullptr;
    };

    EvalResult eval(const std::string& code);
    EvalResult eval_file(const std::string& filepath);

    void* get_symbol_address(const std::string& name);
    std::vector<std::string> get_defined_symbols();

    void add_include_path(const std::string& path);
    void add_library_path(const std::string& path);
    void add_compile_flag(const std::string& flag);
    void set_target_triple(const std::string& triple);

    void reset();
    bool load_pch(const std::string& pch_path);
    bool create_pch(const std::string& header_path, const std::string& output_path);

    [[nodiscard]] std::string get_last_error() const { return m_last_error; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::string m_last_error;

    bool setup_compiler_instance();
    bool create_interpreter();
    std::string preprocess_code(const std::string& code);
    void extract_symbols_from_code(const std::string& code);
    void detect_system_includes();
};

} // namespace Lila
