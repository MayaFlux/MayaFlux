#pragma once

#include "Domain.hpp"
#include "Registry.hpp"

namespace MayaFlux {

struct CreationContext {
    std::optional<Domain> domain;
    std::optional<uint32_t> channel;
    std::optional<std::vector<uint32_t>> channels;

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

MAYAFLUX_API void register_node(const std::shared_ptr<Nodes::Node>& node, const CreationContext& ctx);
MAYAFLUX_API void register_buffer(const std::shared_ptr<Buffers::Buffer>& buffer, const CreationContext& ctx);
MAYAFLUX_API void register_container(const std::shared_ptr<Kakshya::SoundFileContainer>& container, const Domain& domain);

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
        m_ctx.domain = d;
        apply_if_ready();
        return *this;
    }

    CreationHandle& channel(uint32_t ch)
    {
        m_ctx.channel = ch;
        m_ctx.channels.reset();
        apply_if_ready();
        return *this;
    }

    CreationHandle& channels(std::vector<uint32_t> ch)
    {
        m_ctx.channels = std::move(ch);
        m_ctx.channel.reset();
        apply_if_ready();
        return *this;
    }

    template <typename... Args>
    CreationHandle& channels(Args... args)
    {
        static_assert(sizeof...(args) > 0, "channels() requires at least one argument");
        static_assert((std::is_convertible_v<Args, uint32_t> && ...),
            "All arguments must be convertible to uint32_t");

        m_ctx.channels = std::vector<uint32_t> { static_cast<uint32_t>(args)... };
        m_ctx.channel.reset();
        apply_if_ready();
        return *this;
    }

    CreationHandle& operator|(Domain d) { return domain(d); }
    CreationHandle& operator[](uint32_t ch) { return channel(ch); }
    CreationHandle& operator[](std::initializer_list<uint32_t> ch_list)
    {
        m_ctx.channels = std::vector<uint32_t>(ch_list);
        m_ctx.channel.reset();
        apply_if_ready();
        return *this;
    }

private:
    void apply_if_ready()
    {
        if constexpr (std::is_base_of_v<Kakshya::SignalSourceContainer, T>) {
            if (m_ctx.domain) {
                apply_container_context();
                m_ctx = CreationContext {};
            }
        } else if (m_ctx.domain && (m_ctx.channel || m_ctx.channels)) {
            if constexpr (std::is_base_of_v<Nodes::Node, T>) {
                if (m_ctx.domain) {
                    apply_node_context();
                }
            } else if constexpr (std::is_base_of_v<Buffers::Buffer, T>) {
                if (m_ctx.domain) {
                    apply_buffer_context();
                }
            }
            m_ctx = CreationContext {};
        }
    }

    void apply_node_context()
    {
        if (!*this)
            return;

        std::shared_ptr<Nodes::Node> node = std::static_pointer_cast<Nodes::Node>(*this);
        register_node(node, m_ctx);
    }

    void apply_buffer_context()
    {
        if (!*this)
            return;

        std::shared_ptr<Buffers::Buffer> buffer = std::static_pointer_cast<Buffers::Buffer>(*this);
        register_buffer(buffer, m_ctx);
    }

    void apply_container_context()
    {
        if (!*this)
            return;

        std::shared_ptr<Kakshya::SoundFileContainer> container = std::static_pointer_cast<Kakshya::SoundFileContainer>(*this);
        register_container(container, m_ctx.domain.value());
    }

    mutable CreationContext m_ctx;
};

class MAYAFLUX_API Creator {
public:
#define X(method_name, full_type_name)                                            \
    template <typename... Args>                                                   \
    auto method_name(Args&&... args) -> CreationHandle<full_type_name>            \
    {                                                                             \
        auto obj = std::make_shared<full_type_name>(std::forward<Args>(args)...); \
        return CreationHandle<full_type_name>(obj);                               \
    }
    ALL_NODE_REGISTRATIONS
#undef X

#define B(method_name, full_type_name)                                            \
    template <typename... Args>                                                   \
    auto method_name(Args&&... args) -> CreationHandle<full_type_name>            \
    {                                                                             \
        auto obj = std::make_shared<full_type_name>(std::forward<Args>(args)...); \
        return CreationHandle<full_type_name>(obj);                               \
    }
    ALL_BUFFER_REGISTRATION
#undef B

    auto read(const std::string& filepath) -> CreationHandle<Kakshya::SoundFileContainer>
    {
        auto container = load_container(filepath);
        return CreationHandle<Kakshya::SoundFileContainer>(container);
    }

private:
    std::shared_ptr<Kakshya::SoundFileContainer> load_container(const std::string& filepath);
};

template <typename T>
auto operator|(std::shared_ptr<T> obj, Domain d) -> CreationHandle<T>
{
    CreationHandle<T> handle(obj);
    return handle.domain(d);
}

/**
 * @brief Domain constant for Audio domain.
 *
 * This domain unwraps to Nodes::ProccingToken::AUDIO_RATE | Buffers::ProcessingToken::AUDIO_BACKEND | Vruta::ProcessingToken::SAMPLE_ACCURATE;
 */
static constexpr auto Audio = Domain::AUDIO;

/**
 * @brief Domain constant for Graphics domain.
 *
 * This domain unwraps to Nodes::ProccingToken::FRAME_RATE | Buffers::ProcessingToken::GRAPHICS_BACKEND | Vruta::ProcessingToken::FRAME_ACCURATE;
 */
static constexpr auto Graphics = Domain::GRAPHICS;

/**
 * @brief Global Creator instance for creating nodes, buffers, and containers.
 *
 * This instance provides a convenient interface to create various MayaFlux components
 * such as nodes, buffers, and signal source containers. It supports method chaining
 * for setting creation context like domain and channels.
 * Each object can be registered automatically based on the provided context.
 * The contexts include domain, channel, and metadata.
 * Domains supported are AUDIO and GRAPHICS.
 * For example:
 * ```
 * auto myNode = ::vega.sine(440.0f).domain(Audio).channel(0);
 * auto node_buffer = vega.node(0, 512, myNode)[{0, 1, 2}] | Graphics;
 * ```
 */
extern MAYAFLUX_API Creator vega;

} // namespace MayaFlux
