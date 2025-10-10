#pragma once
#include "Domain.hpp"
#include "Registry.hpp"

namespace MayaFlux {

struct CreationContext {
    std::optional<Domain> domain;
    std::optional<uint32_t> channel;
    std::optional<std::vector<uint32_t>> channels;
    std::map<std::string, std::any> metadata;

    CreationContext() = default;

    CreationContext(Domain d)
        : domain(d)
    {
    }

    CreationContext(Domain d, uint32_t ch)
        : domain(d)
        , channel(ch)
    {
    }

    CreationContext(uint32_t ch)
        : channel(ch)
    {
    }

    CreationContext(std::vector<uint32_t> ch)
        : channels(std::move(ch))
    {
    }
};

class Creator;

class CreationProxy {
public:
    CreationProxy(Creator* creator)
        : m_creator(creator)
    {
    }
    CreationProxy(Creator* creator, CreationContext ctx)
        : m_creator(creator)
        , m_context(ctx)
    {
    }

    CreationProxy& domain(Domain d)
    {
        m_context.domain = d;
        return *this;
    }

    CreationProxy& channel(uint32_t ch)
    {
        m_context.channel = ch;
        return *this;
    }

    CreationProxy& channels(std::vector<uint32_t> ch)
    {
        m_context.channels = ch;
        m_context.channel.reset();
        return *this;
    }

    template <typename... Args>
    CreationProxy& channels(Args... args)
    {
        static_assert(sizeof...(args) > 0, "channels() requires at least one argument");
        static_assert((std::is_convertible_v<Args, uint32_t> && ...), "All arguments must be convertible to uint32_t");

        m_context.channels = std::vector<uint32_t> { static_cast<uint32_t>(args)... };
        m_context.channel.reset();
        return *this;
    }

    CreationProxy& with(const std::string& key, std::any value)
    {
        m_context.metadata[key] = std::move(value);
        return *this;
    }

    CreationProxy& operator|(Domain d) { return domain(d); }
    CreationProxy& operator[](uint32_t ch) { return channel(ch); }

    CreationProxy& operator[](std::initializer_list<uint32_t> ch_list)
    {
        m_context.channels = std::vector<uint32_t>(ch_list);
        m_context.channel.reset();
        return *this;
    }

#define DECLARE_NODE_CREATION_METHOD(method_name, full_type_name)                 \
    template <typename... Args>                                                   \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>           \
    {                                                                             \
        auto obj = create_node_impl<full_type_name>(std::forward<Args>(args)...); \
        apply_node_context(obj);                                                  \
        return obj;                                                               \
    }

#define X(method_name, full_type_name) DECLARE_NODE_CREATION_METHOD(method_name, full_type_name)
    ALL_NODE_REGISTRATIONS
#undef X

#define DECLARE_BUFFER_CREATION_METHOD(method_name, full_type_name)                 \
    template <typename... Args>                                                     \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>             \
    {                                                                               \
        auto obj = create_buffer_impl<full_type_name>(std::forward<Args>(args)...); \
        apply_buffer_context(obj);                                                  \
        return obj;                                                                 \
    }

#define B(method_name, full_type_name) DECLARE_BUFFER_CREATION_METHOD(method_name, full_type_name)
    ALL_BUFFER_REGISTRATION
#undef B

    auto read(const std::string& filepath) -> std::shared_ptr<Kakshya::SoundFileContainer>;

private:
    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_impl(Args&&... args);

    template <typename T, typename... Args>
    std::shared_ptr<T> create_buffer_impl(Args&&... args);

    std::shared_ptr<Kakshya::SoundFileContainer> create_container_impl(const std::string& filepath);

    template <typename T>
    void apply_node_context(std::shared_ptr<T> obj);

    template <typename T>
    void apply_buffer_context(std::shared_ptr<T> obj);

    void apply_container_context(std::shared_ptr<Kakshya::SoundFileContainer> container);

    CreationContext m_context;
    Creator* m_creator;
};

class ContextAppliers {
public:
    static auto& get_node_context_applier()
    {
        // static std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)> applier;
        // return applier;
        return s_node_applier;
    }

    static auto& get_buffer_context_applier()
    {
        // static std::function<void(std::shared_ptr<Buffers::Buffer>, const CreationContext&)> applier;
        // return applier;
        return s_buffer_applier;
    }

    static auto& get_container_context_applier()
    {
        // static std::function<void(std::shared_ptr<Kakshya::SoundFileContainer>, const CreationContext&)> applier;
        // return applier;
        return s_container_applier;
    }

    static auto& get_container_loader()
    {
        // static std::function<std::shared_ptr<Kakshya::SoundFileContainer>(const std::string&)> loader;
        // return loader;
        return s_loader;
    }

    static void set_node_context_applier(std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)> applier)
    {
        get_node_context_applier() = std::move(applier);
    }

    static void set_buffer_context_applier(std::function<void(std::shared_ptr<Buffers::Buffer>, const CreationContext&)> applier)
    {
        get_buffer_context_applier() = std::move(applier);
    }

    static void set_container_context_applier(std::function<void(std::shared_ptr<Kakshya::SoundFileContainer>, const CreationContext&)> applier)
    {
        get_container_context_applier() = std::move(applier);
    }

    static void set_container_loader(std::function<std::shared_ptr<Kakshya::SoundFileContainer>(const std::string&)> loader)
    {
        get_container_loader() = std::move(loader);
    }

    // Workaround for JIT:
private:
    static std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)> s_node_applier;
    static std::function<void(std::shared_ptr<Buffers::Buffer>, const CreationContext&)> s_buffer_applier;
    static std::function<void(std::shared_ptr<Kakshya::SoundFileContainer>, const CreationContext&)> s_container_applier;
    static std::function<std::shared_ptr<Kakshya::SoundFileContainer>(const std::string&)> s_loader;
};

template <typename T>
class CreationHandle : public std::shared_ptr<T> {
public:
    using std::shared_ptr<T>::shared_ptr;

    CreationHandle(const std::shared_ptr<T>& ptr)
        : std::shared_ptr<T>(ptr)
    {
    }
    CreationHandle(std::shared_ptr<T>&& ptr)
        : std::shared_ptr<T>(std::move(ptr))
    {
    }

    CreationHandle& domain(Domain d)
    {
        m_accumulated_context.domain = d;
        try_apply_context();
        return *this;
    }

    CreationHandle& channel(uint32_t ch)
    {
        m_accumulated_context.channel = ch;
        try_apply_context();
        return *this;
    }

    CreationHandle& channels(std::vector<uint32_t> ch)
    {
        m_accumulated_context.channels = ch;
        m_accumulated_context.channel.reset();
        return *this;
    }

    template <typename... Args>
    CreationProxy& channels(Args... args)
    {
        static_assert(sizeof...(args) > 0, "channels() requires at least one argument");
        static_assert((std::is_convertible_v<Args, uint32_t> && ...), "All arguments must be convertible to uint32_t");

        m_accumulated_context.channels = std::vector<uint32_t> { static_cast<uint32_t>(args)... };
        m_accumulated_context.channel.reset();
        return *this;
    }

    CreationHandle& with(const std::string& key, std::any value)
    {
        m_accumulated_context.metadata[key] = std::move(value);
        try_apply_context();
        return *this;
    }

    CreationHandle& operator|(Domain d) { return domain(d); }
    CreationHandle& operator[](uint32_t ch) { return channel(ch); }

    CreationHandle& operator[](std::initializer_list<uint32_t> ch_list)
    {
        m_accumulated_context.channels = std::vector<uint32_t>(ch_list);
        m_accumulated_context.channel.reset();
        return *this;
    }

private:
    void try_apply_context() const
    {
        if constexpr (std::is_base_of_v<Kakshya::SignalSourceContainer, T>) {
            if (m_accumulated_context.domain) {
                if (auto& applier = ContextAppliers::get_container_context_applier()) {
                    applier(std::static_pointer_cast<T>(*this), m_accumulated_context);
                }
                m_accumulated_context = CreationContext {};
            }
        } else {
            if (m_accumulated_context.domain && (m_accumulated_context.channel || m_accumulated_context.channels)) {
                if constexpr (std::is_base_of_v<Nodes::Node, T>) {
                    if (auto& applier = ContextAppliers::get_node_context_applier()) {
                        applier(std::static_pointer_cast<Nodes::Node>(*this), m_accumulated_context);
                    }
                } else if constexpr (std::is_base_of_v<Buffers::Buffer, T>) {
                    if (auto& applier = ContextAppliers::get_buffer_context_applier()) {
                        applier(std::static_pointer_cast<Buffers::Buffer>(*this), m_accumulated_context);
                    }
                }

                m_accumulated_context = CreationContext {};
            }
        }
    }

    mutable CreationContext m_accumulated_context;
};

class Creator {
public:
#define DECLARE_NODE_CREATOR_METHOD(method_name, full_type_name)                  \
    template <typename... Args>                                                   \
    auto method_name(Args&&... args) -> CreationHandle<full_type_name>            \
    {                                                                             \
        auto obj = create_node_impl<full_type_name>(std::forward<Args>(args)...); \
        return CreationHandle<full_type_name>(obj);                               \
    }

#define X(method_name, full_type_name) DECLARE_NODE_CREATOR_METHOD(method_name, full_type_name)
    ALL_NODE_REGISTRATIONS
#undef X

#define DECLARE_BUFFER_CREATOR_METHOD(method_name, full_type_name)                  \
    template <typename... Args>                                                     \
    auto method_name(Args&&... args) -> CreationHandle<full_type_name>              \
    {                                                                               \
        auto obj = create_buffer_impl<full_type_name>(std::forward<Args>(args)...); \
        return CreationHandle<full_type_name>(obj);                                 \
    }

#define B(method_name, full_type_name) DECLARE_BUFFER_CREATOR_METHOD(method_name, full_type_name)
    ALL_BUFFER_REGISTRATION
#undef B

    auto read(const std::string& filepath) -> CreationHandle<Kakshya::SoundFileContainer>
    {
        if (auto& loader = ContextAppliers::get_container_loader()) {
            auto container = loader(filepath);
            return CreationHandle<Kakshya::SoundFileContainer>(container);
        }
        return CreationHandle<Kakshya::SoundFileContainer>(nullptr);
    }

    CreationProxy domain(Domain d)
    {
        return CreationProxy(this, CreationContext(d));
    }

    CreationProxy channel(uint32_t ch)
    {
        return CreationProxy(this, CreationContext(ch));
    }

    CreationProxy channels(std::vector<uint32_t> ch)
    {
        return CreationProxy(this, CreationContext(ch));
    }

    template <typename... Args>
    CreationProxy channels(Args... args)
    {
        static_assert(sizeof...(args) > 0, "channels() requires at least one argument");
        static_assert((std::is_convertible_v<Args, uint32_t> && ...), "All arguments must be convertible to uint32_t");

        auto ch = std::vector<uint32_t> { static_cast<uint32_t>(args)... };
        return CreationProxy(this, CreationContext(ch));
    }

    CreationProxy with(const std::string& key, std::any value)
    {
        CreationContext ctx;
        ctx.metadata[key] = std::move(value);
        return CreationProxy(this, ctx);
    }

    CreationProxy operator|(Domain d) { return domain(d); }
    CreationProxy operator[](uint32_t ch) { return channel(ch); }

    CreationProxy operator[](std::initializer_list<uint32_t> ch_list)
    {
        return channels(std::vector<uint32_t>(ch_list));
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_for_proxy(Args&&... args)
    {
        return create_node_impl<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> create_buffer_for_proxy(Args&&... args)
    {
        return create_buffer_impl<T>(std::forward<Args>(args)...);
    }

    std::shared_ptr<Kakshya::SoundFileContainer> create_container_for_proxy(const std::string& filepath)
    {
        if (auto& loader = ContextAppliers::get_container_loader()) {
            return loader(filepath);
        }
        return nullptr;
    }

private:
    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_impl(Args&&... args)
    {
        try {
            return std::make_shared<T>(std::forward<Args>(args)...);
        } catch (...) {
            return nullptr;
        }
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> create_buffer_impl(Args&&... args)
    {
        try {
            return std::make_shared<T>(std::forward<Args>(args)...);
        } catch (...) {
            return nullptr;
        }
    }
};

template <typename T, typename... Args>
std::shared_ptr<T> CreationProxy::create_node_impl(Args&&... args)
{
    return m_creator->create_node_for_proxy<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
std::shared_ptr<T> CreationProxy::create_buffer_impl(Args&&... args)
{
    return m_creator->create_buffer_for_proxy<T>(std::forward<Args>(args)...);
}

template <typename T>
void CreationProxy::apply_node_context(std::shared_ptr<T> obj)
{
    if (!obj)
        return;
    if (auto& applier = ContextAppliers::get_node_context_applier()) {
        applier(std::static_pointer_cast<Nodes::Node>(obj), m_context);
    }
}

template <typename T>
void CreationProxy::apply_buffer_context(std::shared_ptr<T> obj)
{
    if (!obj)
        return;
    if (auto& applier = ContextAppliers::get_buffer_context_applier()) {
        applier(std::static_pointer_cast<Buffers::Buffer>(obj), m_context);
    }
}

static constexpr auto Audio = Domain::AUDIO;
static constexpr auto Graphics = Domain::GRAPHICS;

inline Creator vega;

template <typename T>
auto operator|(std::shared_ptr<T> obj, Domain d) -> CreationHandle<T>
{
    CreationHandle<T> handle(obj);
    return handle.domain(d);
}

} // namespace MayaFlux
