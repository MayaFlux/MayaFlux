#pragma once

namespace MayaFlux::IO {

template <typename Writer>
using WriterFactory = std::function<std::unique_ptr<Writer>()>;

/**
 * @class WriterRegistry
 * @brief Singleton registry dispatching single-shot writes by file extension.
 *
 * Shared by every single-shot writer family (ImageWriter, ModelWriter,
 * VolumeWriter, ...). Concrete writers register themselves during subsystem
 * init via register_writer(); create_writer(path) looks up the extension
 * and returns a fresh instance, or nullptr if none is registered.
 *
 * The nullptr is the whole point of the indirection: a format whose backing
 * library is not present on a given build simply has no entry, and the
 * caller gets a logged miss at the call site rather than a link error at
 * startup.
 *
 * @tparam Writer Abstract single-shot writer type this registry dispatches.
 */
template <typename Writer>
class WriterRegistry {
public:
    static WriterRegistry& instance()
    {
        static WriterRegistry registry;
        return registry;
    }

    void register_writer(
        const std::vector<std::string>& extensions,
        const WriterFactory<Writer>& factory)
    {
        for (const auto& ext : extensions) {
            m_factories[ext] = factory;
        }
    }

    [[nodiscard]] std::unique_ptr<Writer> create_writer(const std::string& filepath) const
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }

        auto it = m_factories.find(ext);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> get_registered_extensions() const
    {
        std::vector<std::string> exts;
        exts.reserve(m_factories.size());
        for (const auto& [ext, _] : m_factories) {
            exts.push_back(ext);
        }
        return exts;
    }

private:
    std::unordered_map<std::string, WriterFactory<Writer>> m_factories;
};

} // namespace MayaFlux::IO
