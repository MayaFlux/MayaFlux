#include "Scan.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr const char* marker_source = R"(#define MAYAFLUX_REFLECT_SCAN
#ifdef MF_DESCRIBE
#undef MF_DESCRIBE
#endif
#define MF_REFLECT_JOIN_IMPL(a, b) a##b
#define MF_REFLECT_JOIN(a, b) MF_REFLECT_JOIN_IMPL(a, b)
#define MF_DESCRIBE(...) using MF_REFLECT_JOIN(mf_reflection_request_, __COUNTER__) = __VA_ARGS__;
)";

    std::filesystem::path absolute_of(const std::string& file)
    {
        std::error_code code;
        auto resolved = std::filesystem::weakly_canonical(file, code);
        return code ? std::filesystem::path(file) : resolved;
    }

    std::filesystem::path select_translation_unit(
        const clang::tooling::CompilationDatabase& database,
        const ScanInput& input)
    {
        if (!input.translation_unit.empty()) {
            return absolute_of(input.translation_unit.string());
        }
        const std::vector<std::string> files = database.getAllFiles();
        for (const std::string& file : files) {
            const std::filesystem::path candidate = absolute_of(file);
            for (const auto& root : input.roots) {
                const auto relative = candidate.lexically_relative(root);
                if (!relative.empty() && *relative.begin() != "..") {
                    return candidate;
                }
            }
        }
        return files.empty() ? std::filesystem::path {} : absolute_of(files.front());
    }

    std::string scan_source(const std::vector<std::filesystem::path>& headers)
    {
        std::string source = marker_source;
        for (const auto& header : headers) {
            source += "#include \"" + header.generic_string() + "\"\n";
        }
        return source;
    }

    clang::tooling::ArgumentsAdjuster make_adjuster(const ScanInput& input)
    {
        return [resource = input.resource_dir, extra = input.extra_args](
                   const clang::tooling::CommandLineArguments& arguments, llvm::StringRef) {
            clang::tooling::CommandLineArguments adjusted;
            adjusted.reserve(arguments.size() + extra.size() + 2);
            bool has_resource = false;
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                const std::string& argument = arguments[i];
                if (argument == "-Xclang" && i + 3 < arguments.size() && arguments[i + 1] == "-include-pch") {
                    i += 3;
                    continue;
                }
                if (argument == "-include-pch") {
                    ++i;
                    continue;
                }
                has_resource = has_resource || argument == "-resource-dir";
                adjusted.push_back(argument);
            }
            const auto insert_at = adjusted.begin() + (adjusted.empty() ? 0 : 1);
            std::vector<std::string> added;
            if (!has_resource && !resource.empty() && llvm::sys::fs::is_directory(resource)) {
                added.emplace_back("-resource-dir");
                added.push_back(resource);
            }
            added.insert(added.end(), extra.begin(), extra.end());
            adjusted.insert(insert_at, added.begin(), added.end());
            return adjusted;
        };
    }

    struct TemporaryDirectory {
        std::filesystem::path path;

        explicit TemporaryDirectory(std::filesystem::path directory)
            : path(std::move(directory))
        {
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        TemporaryDirectory(TemporaryDirectory&&) = delete;
        TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

        ~TemporaryDirectory()
        {
            std::error_code code;
            std::filesystem::remove_all(path, code);
        }
    };

    class ScanDatabase : public clang::tooling::CompilationDatabase {
    public:
        explicit ScanDatabase(clang::tooling::CompileCommand command)
            : m_command(std::move(command))
        {
        }

        [[nodiscard]] std::vector<clang::tooling::CompileCommand> getCompileCommands(llvm::StringRef file) const override
        {
            if (file == m_command.Filename) {
                return { m_command };
            }
            return {};
        }

        [[nodiscard]] std::vector<std::string> getAllFiles() const override { return { m_command.Filename }; }

        [[nodiscard]] std::vector<clang::tooling::CompileCommand> getAllCompileCommands() const override
        {
            return { m_command };
        }

    private:
        clang::tooling::CompileCommand m_command;
    };

    bool retarget(clang::tooling::CompileCommand& command, const std::filesystem::path& unit,
        const std::filesystem::path& scan_file)
    {
        bool replaced = false;
        for (std::string& argument : command.CommandLine) {
            if (argument.empty() || argument.front() == '-') {
                continue;
            }
            std::filesystem::path candidate(argument);
            if (candidate.is_relative()) {
                candidate = std::filesystem::path(command.Directory) / candidate;
            }
            if (absolute_of(candidate.string()) == unit) {
                argument = scan_file.string();
                replaced = true;
            }
        }
        command.Filename = scan_file.string();
        return replaced;
    }

}

bool scan(const ScanInput& input, RequestCollector& collector, std::string& error)
{
    if (input.build_path.empty()) {
        error = "a compile database directory is required (-p)";
        return false;
    }

    std::string load_error;
    const auto database = clang::tooling::CompilationDatabase::loadFromDirectory(input.build_path.string(), load_error);
    if (!database) {
        error = "cannot load compile_commands.json from " + input.build_path.string() + ": " + load_error;
        return false;
    }

    const std::filesystem::path unit = select_translation_unit(*database, input);
    const auto commands = unit.empty() ? std::vector<clang::tooling::CompileCommand> {}
                                       : database->getCompileCommands(unit.string());
    if (commands.empty()) {
        error = "no compile command found for translation unit " + unit.string();
        return false;
    }

    llvm::SmallString<128> temporary;
    if (llvm::sys::fs::createUniqueDirectory("mayaflux-reflect", temporary)) {
        error = "cannot create a temporary directory for the scan source";
        return false;
    }
    const TemporaryDirectory cleanup(std::filesystem::path(temporary.str().str()));

    const std::filesystem::path scan_file = cleanup.path / "mf_reflect_scan.cpp";
    {
        std::ofstream source(scan_file, std::ios::binary | std::ios::trunc);
        source << scan_source(input.headers);
        if (!source) {
            error = "cannot write " + scan_file.string();
            return false;
        }
    }

    clang::tooling::CompileCommand command = commands.front();
    if (!retarget(command, unit, scan_file)) {
        error = "the compile command of " + unit.string() + " does not name its source file";
        return false;
    }

    std::cerr << "Scanning " << input.headers.size() << " header(s) with the compile command of " << unit.string() << '\n';

    const ScanDatabase scan_database(std::move(command));
    clang::tooling::ClangTool tool(scan_database, { scan_file.string() });
    tool.appendArgumentsAdjuster(make_adjuster(input));

    clang::ast_matchers::MatchFinder finder;
    collector.attach(finder);

    if (tool.run(clang::tooling::newFrontendActionFactory(&finder).get()) != 0) {
        error = "Clang could not scan the selected headers (translation unit " + unit.string() + ")";
        return false;
    }
    return true;
}

}
