#pragma once
#include "Domain.hpp"
#include "NodeRegistry.hpp"

namespace MayaFlux {

template <typename T>
concept NodeType = std::is_base_of_v<Nodes::Node, T>;

struct CreationContext {
    std::optional<Domain> domain;
    std::optional<uint32_t> channel;
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
};

class Creator;

#define DECLARE_NODE_CREATOR_SMART(method_name, full_type_name)                    \
    template <typename... Args>                                                    \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>            \
    {                                                                              \
        auto node = create_node_impl<full_type_name>(std::forward<Args>(args)...); \
        return node;                                                               \
    }

#define DECLARE_NODE_PROXY_METHOD(method_name, full_type_name)                     \
    template <typename... Args>                                                    \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>            \
    {                                                                              \
        auto node = create_node_impl<full_type_name>(std::forward<Args>(args)...); \
        apply_context_to_node(node);                                               \
        return node;                                                               \
    }

#define REGISTER_NODE_TYPE(method_name, full_type_name) \
public:                                                 \
    DECLARE_NODE_CREATOR_SMART(method_name, full_type_name)

#define ADD_TO_NODE_PROXY(method_name, full_type_name) \
    DECLARE_NODE_PROXY_METHOD(method_name, full_type_name)

class NodeProxy {
private:
    CreationContext m_context;
    Creator* m_creator;

public:
    NodeProxy(Creator* creator)
        : m_creator(creator)
    {
    }
    NodeProxy(Creator* creator, CreationContext ctx)
        : m_creator(creator)
        , m_context(ctx)
    {
    }

    NodeProxy& domain(Domain d)
    {
        m_context.domain = d;
        return *this;
    }

    NodeProxy& channel(uint32_t ch)
    {
        m_context.channel = ch;
        return *this;
    }

    NodeProxy& with(const std::string& key, std::any value)
    {
        m_context.metadata[key] = std::move(value);
        return *this;
    }

    NodeProxy& operator|(Domain d)
    {
        return domain(d);
    }

    NodeProxy& operator[](uint32_t ch)
    {
        return channel(ch);
    }

#define X(method_name, full_type_name) ADD_TO_NODE_PROXY(method_name, full_type_name)
    ALL_NODE_REGISTRATIONS
#undef X

private:
    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_impl(Args&&... args);

    template <typename T>
    void apply_context_to_node(std::shared_ptr<T> node)
    {
        if (!node) {
            std::cerr << "Warning: Node creation failed" << std::endl;
            return;
        }

        auto& applier = get_context_applier();
        if (applier) {
            applier(std::static_pointer_cast<Nodes::Node>(node), m_context);
        }
    }

public:
    static std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)>& get_context_applier()
    {
        static std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)> applier;
        return applier;
    }

    static void set_context_applier(std::function<void(std::shared_ptr<Nodes::Node>, const CreationContext&)> func)
    {
        get_context_applier() = func;
    }
};

template <typename T>
class NodeHandle : public std::shared_ptr<T> {
private:
    mutable CreationContext m_accumulated_context;

public:
    using std::shared_ptr<T>::shared_ptr;

    NodeHandle(const std::shared_ptr<T>& ptr)
        : std::shared_ptr<T>(ptr)
    {
    }
    NodeHandle(std::shared_ptr<T>&& ptr)
        : std::shared_ptr<T>(std::move(ptr))
    {
    }

    NodeHandle& domain(Domain d)
    {
        m_accumulated_context.domain = d;
        try_apply_context();
        return *this;
    }

    NodeHandle& channel(uint32_t ch)
    {
        m_accumulated_context.channel = ch;
        try_apply_context();
        return *this;
    }

    NodeHandle& with(const std::string& key, std::any value)
    {
        m_accumulated_context.metadata[key] = std::move(value);
        try_apply_context();
        return *this;
    }

    NodeHandle& operator|(Domain d) { return domain(d); }
    NodeHandle& operator[](uint32_t ch) { return channel(ch); }

private:
    void try_apply_context() const
    {
        if (m_accumulated_context.domain && m_accumulated_context.channel) {
            if (auto& applier = NodeProxy::get_context_applier()) {
                applier(*this, m_accumulated_context);
                m_accumulated_context = CreationContext {};
            }
        }
    }
};

class Creator {
private:
    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_impl(Args&&... args)
    {
        try {
            return std::make_shared<T>(std::forward<Args>(args)...);
        } catch (...) {
            try {
                return std::make_shared<T>();
            } catch (...) {
                return nullptr;
            }
        }
    }

public:
#define DECLARE_FLUENT_CREATOR_METHOD(method_name, full_type_name)                 \
    template <typename... Args>                                                    \
    auto method_name(Args&&... args) -> NodeHandle<full_type_name>                 \
    {                                                                              \
        auto node = create_node_impl<full_type_name>(std::forward<Args>(args)...); \
        return NodeHandle<full_type_name>(node);                                   \
    }

#define X(method_name, full_type_name) DECLARE_FLUENT_CREATOR_METHOD(method_name, full_type_name)
    ALL_NODE_REGISTRATIONS
#undef X

    NodeProxy domain(Domain d)
    {
        return NodeProxy(this, CreationContext(d));
    }

    NodeProxy channel(uint32_t ch)
    {
        CreationContext ctx;
        ctx.channel = ch;
        return NodeProxy(this, ctx);
    }

    NodeProxy with(const std::string& key, std::any value)
    {
        CreationContext ctx;
        ctx.metadata[key] = std::move(value);
        return NodeProxy(this, ctx);
    }

    NodeProxy operator|(Domain d)
    {
        return domain(d);
    }

    NodeProxy operator[](uint32_t ch)
    {
        return channel(ch);
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> create_node_for_proxy(Args&&... args)
    {
        return create_node_impl<T>(std::forward<Args>(args)...);
    }
};

template <typename T, typename... Args>
std::shared_ptr<T> NodeProxy::create_node_impl(Args&&... args)
{
    return m_creator->create_node_for_proxy<T>(std::forward<Args>(args)...);
}

static constexpr auto Audio = Domain::AUDIO;
static constexpr auto Graphics = Domain::GRAPHICS;

inline Creator create;

template <typename T>
auto operator|(std::shared_ptr<T> node, Domain d) -> NodeHandle<T>
{
    NodeHandle<T> handle(node);
    return handle.domain(d);
}

} // namespace MayaFlux
