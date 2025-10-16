#pragma once

namespace clang {
class Interpreter;
class CompilerInstance;
}

namespace Lila {

/**
 * @class ClangInterpreter
 * @brief Embedded Clang interpreter for live code evaluation in MayaFlux
 *
 * The ClangInterpreter class provides an interface for compiling and executing C++ code snippets
 * at runtime using Clang's incremental interpreter. It is the core engine behind Lila's live coding
 * capabilities, enabling real-time code evaluation, symbol lookup, and dynamic integration of user code.
 *
 * ## Core Responsibilities
 * - Initialize and manage the Clang interpreter instance
 * - Evaluate code snippets and files, returning results and errors
 * - Track and expose defined symbols for runtime introspection
 * - Allow configuration of include paths, compile flags, and target triple
 * - Provide error reporting and reset functionality
 *
 * ## Usage Flow
 * 1. Construct a ClangInterpreter instance
 * 2. Call initialize() to set up the interpreter
 * 3. Use eval() or eval_file() to execute code
 * 4. Query symbols or errors as needed
 * 5. Optionally configure include paths, flags, or target triple before initialization
 * 6. Call shutdown() or reset() to clean up
 *
 * This class is intended for internal use by Lila and is not exposed to end users directly.
 */
class ClangInterpreter {
public:
    /**
     * @brief Constructs a ClangInterpreter instance
     */
    ClangInterpreter();

    /**
     * @brief Destructor; shuts down the interpreter if active
     */
    ~ClangInterpreter();

    ClangInterpreter(const ClangInterpreter&) = delete;
    ClangInterpreter& operator=(const ClangInterpreter&) = delete;
    ClangInterpreter(ClangInterpreter&&) noexcept;
    ClangInterpreter& operator=(ClangInterpreter&&) noexcept;

    /**
     * @brief Initializes the Clang interpreter and prepares for code evaluation
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize();

    /**
     * @brief Shuts down the interpreter and releases resources
     */
    void shutdown();

    /**
     * @struct EvalResult
     * @brief Result of code evaluation
     *
     * Contains success status, output, error message, and optional symbol address.
     */
    struct EvalResult {
        bool success = false; ///< True if evaluation succeeded
        std::string output; ///< Output from code execution
        std::string error; ///< Error message if evaluation failed
        void* symbol_address = nullptr; ///< Address of defined symbol (if applicable)
    };

    /**
     * @brief Evaluates a code snippet
     * @param code C++ code to execute
     * @return EvalResult containing success, output, and error info
     */
    EvalResult eval(const std::string& code);

    /**
     * @brief Evaluates a code file by including it
     * @param filepath Path to the file to execute
     * @return EvalResult containing success, output, and error info
     */
    EvalResult eval_file(const std::string& filepath);

    /**
     * @brief Gets the address of a symbol defined in the interpreter
     * @param name Symbol name
     * @return Pointer to symbol, or nullptr if not found
     */
    void* get_symbol_address(const std::string& name);

    /**
     * @brief Gets a list of all defined symbols
     * @return Vector of symbol names
     */
    std::vector<std::string> get_defined_symbols();

    /**
     * @brief Adds an include path for code evaluation
     * @param path Directory to add to the include search path
     */
    void add_include_path(const std::string& path);

    /**
     * @brief Adds a library path (not yet implemented)
     * @param path Directory to add to the library search path
     */
    void add_library_path(const std::string& path);

    /**
     * @brief Adds a compile flag for code evaluation
     * @param flag Compiler flag to add
     */
    void add_compile_flag(const std::string& flag);

    /**
     * @brief Sets the target triple for code generation
     * @param triple Target triple string
     */
    void set_target_triple(const std::string& triple);

    /**
     * @brief Resets the interpreter, clearing all state
     */
    void reset();

    /**
     * @brief Gets the last error message
     * @return String containing the last error
     */
    [[nodiscard]] std::string get_last_error() const { return m_last_error; }

private:
    struct Impl; ///< Internal implementation details
    std::unique_ptr<Impl> m_impl;
    std::string m_last_error;

    bool setup_compiler_instance();
    bool create_interpreter();
    std::string preprocess_code(const std::string& code);
    void extract_symbols_from_code(const std::string& code);
    void detect_system_includes();
};

} // namespace Lila
