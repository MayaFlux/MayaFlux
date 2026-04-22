#pragma once

#include "Domain.hpp"
#include "Registry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

namespace Core {
    class VKImage;
}

namespace IO {
    using TextureResolver = std::function<std::shared_ptr<Core::VKImage>(const std::string& path)>;
}

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
    CreationContext(Domain d, std::vector<uint32_t> ch)
        : domain(d)
        , channels(std::move(ch))
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
MAYAFLUX_API void register_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network, const CreationContext& ctx);
MAYAFLUX_API void register_buffer(const std::shared_ptr<Buffers::Buffer>& buffer, const CreationContext& ctx);
MAYAFLUX_API void register_container(const std::shared_ptr<Kakshya::SoundFileContainer>& container, const Domain& domain);

/**
 * @brief Thin domain wrapper that adds subscript channel-binding syntax.
 *
 * Audio[0] and Audio[{0,1}] produce a CreationContext consumed immediately
 * by operator|. Implicit conversion to Domain keeps all existing call sites
 * that accept a plain Domain parameter working without change.
 */
struct DomainSpec {
    Domain value;

    /** @brief Bind a single channel to this domain. */
    [[nodiscard]] CreationContext operator[](uint32_t ch) const
    {
        return { value, ch };
    }

    /** @brief Bind multiple channels to this domain. */
    [[nodiscard]] CreationContext operator[](std::initializer_list<uint32_t> chs) const
    {
        return { value, std::vector<uint32_t>(chs) };
    }

    /** @brief Implicit decay to Domain for call sites that accept Domain directly. */
    operator Domain() const { return value; }

    /** @brief Implicit decay to CreationContext for bare ptr | Audio usage. */
    operator CreationContext() const { return { value }; }
};

class MAYAFLUX_API MeshGroupHandle {
public:
    explicit MeshGroupHandle(std::vector<std::shared_ptr<Buffers::MeshBuffer>> buffers);
    ~MeshGroupHandle();

    MeshGroupHandle& operator|(Domain d);

    auto begin() { return m_buffers.begin(); }
    auto end() { return m_buffers.end(); }
    [[nodiscard]] auto begin() const { return m_buffers.begin(); }
    [[nodiscard]] auto end() const { return m_buffers.end(); }

    [[nodiscard]] bool empty() const { return m_buffers.empty(); }
    [[nodiscard]] size_t size() const { return m_buffers.size(); }

    std::shared_ptr<Buffers::MeshBuffer>& operator[](size_t i) { return m_buffers[i]; }

private:
    std::vector<std::shared_ptr<Buffers::MeshBuffer>> m_buffers;
};

class MAYAFLUX_API Creator {
public:
#define N(method_name, full_type_name)                                        \
    template <typename... Args>                                               \
        requires std::constructible_from<full_type_name, Args...>             \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>       \
    {                                                                         \
        return std::make_shared<full_type_name>(std::forward<Args>(args)...); \
    }
    ALL_NODE_REGISTRATIONS
#undef N

#define W(method_name, full_type_name)                                        \
    template <typename... Args>                                               \
        requires std::constructible_from<full_type_name, Args...>             \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>       \
    {                                                                         \
        return std::make_shared<full_type_name>(std::forward<Args>(args)...); \
    }
    ALL_NODE_NETWORK_REGISTRATIONS
#undef W

#define B(method_name, full_type_name)                                        \
    template <typename... Args>                                               \
        requires std::constructible_from<full_type_name, Args...>             \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>       \
    {                                                                         \
        return std::make_shared<full_type_name>(std::forward<Args>(args)...); \
    }
    ALL_BUFFER_REGISTRATION
#undef B

    auto read_audio(const std::string& filepath) -> std::shared_ptr<Kakshya::SoundFileContainer>
    {
        return load_sound_container(filepath);
    }

    auto read_image(const std::string& filepath) -> std::shared_ptr<Buffers::TextureBuffer>
    {
        return load_image_buffer(filepath);
    }

    auto read_mesh(const std::string& filepath) -> MeshGroupHandle
    {
        return MeshGroupHandle(load_mesh_buffers(filepath));
    }

    auto read_mesh_network(
        const std::string& filepath,
        IO::TextureResolver resolver = nullptr)
        -> std::shared_ptr<Nodes::Network::MeshNetwork>
    {
        return load_mesh_network(filepath, std::move(resolver));
    }

    // ═══════════════════════════════════════════════════════════════
    // Input Node Creation (Special - defined in Creator.cpp)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Create and register HID input node.
     * @param config HID input configuration.
     * @param binding HID input binding.
     * @return Shared pointer to HIDNode (already registered).
     */
    std::shared_ptr<Nodes::Input::HIDNode> read_hid(
        const Nodes::Input::HIDConfig& config,
        const Core::InputBinding& binding);

    /**
     * @brief Create and register MIDI input node.
     * @param config MIDI input configuration.
     * @param binding MIDI input binding.
     * @return Shared pointer to MIDINode (already registered).
     */
    std::shared_ptr<Nodes::Input::MIDINode> read_midi(
        const Nodes::Input::MIDIConfig& config,
        const Core::InputBinding& binding);

    /**
     * @brief Create and register OSC input node.
     * @param config OSC input configuration.
     * @param binding OSC input binding (use InputBinding::osc("/address")).
     * @return Shared pointer to OSCNode (already registered).
     */
    std::shared_ptr<Nodes::Input::OSCNode> read_osc(
        const Nodes::Input::OSCConfig& config,
        const Core::InputBinding& binding);

    /**
     * @brief Create and register generic input node.
     * @param config Generic input configuration.
     * @param binding Generic input binding.
     * @return Shared pointer to InputNode (already registered).
     */
    std::shared_ptr<Nodes::Input::InputNode> read_input(
        const Nodes::Input::InputConfig& config,
        const Core::InputBinding& binding);

private:
    std::shared_ptr<Kakshya::SoundFileContainer> load_sound_container(const std::string& filepath);
    std::shared_ptr<Buffers::TextureBuffer> load_image_buffer(const std::string& filepath);
    std::vector<std::shared_ptr<Buffers::MeshBuffer>> load_mesh_buffers(const std::string& filepath);
    std::shared_ptr<Nodes::Network::MeshNetwork> load_mesh_network(const std::string& filepath, IO::TextureResolver resolver);
};

// ═══════════════════════════════════════════════════════════════
// New pipe operators -- shared_ptr<T> | CreationContext
// DomainSpec converts implicitly to CreationContext so
// | Audio, | Audio[0], | Audio[{0,1}], | Graphics all route here.
// ═══════════════════════════════════════════════════════════════

template <typename T>
    requires std::is_base_of_v<Nodes::Node, T>
std::shared_ptr<T> operator|(std::shared_ptr<T> obj, const CreationContext& ctx)
{
    register_node(std::static_pointer_cast<Nodes::Node>(obj), ctx);
    return obj;
}

template <typename T>
    requires std::is_base_of_v<Nodes::Network::NodeNetwork, T>
std::shared_ptr<T> operator|(std::shared_ptr<T> obj, const CreationContext& ctx)
{
    register_network(std::static_pointer_cast<Nodes::Network::NodeNetwork>(obj), ctx);
    return obj;
}

template <typename T>
    requires std::is_base_of_v<Buffers::Buffer, T>
std::shared_ptr<T> operator|(std::shared_ptr<T> obj, const CreationContext& ctx)
{
    register_buffer(std::static_pointer_cast<Buffers::Buffer>(obj), ctx);
    return obj;
}

inline std::shared_ptr<Kakshya::SoundFileContainer> operator|(
    std::shared_ptr<Kakshya::SoundFileContainer> obj,
    const CreationContext& ctx)
{
    if (ctx.domain)
        register_container(obj, ctx.domain.value());
    return obj;
}

/**
 * @brief Domain constant for Audio domain.
 *
 * Supports subscript syntax for channel binding:
 * @code
 * auto wave = vega.Sine(440.f) | Audio[0];
 * auto net  = vega.ModalNetwork(16, 220.0) | Audio[{0, 1}];
 * @endcode
 */
static constexpr DomainSpec Audio { .value = Domain::AUDIO };

/**
 * @brief Domain constant for Graphics domain.
 *
 * @code
 * auto tex = vega.TextureBuffer(...) | Graphics;
 * @endcode
 */
static constexpr DomainSpec Graphics { .value = Domain::GRAPHICS };

/**
 * @brief Global Creator instance.
 *
 * @code
 * auto wave = vega.Sine(440.f) | Audio[0];
 * auto buf  = vega.AudioBuffer(0, 512) | Audio[0];
 * auto net  = vega.ModalNetwork(16, 220.0) | Audio[{0, 1}];
 * auto tex  = vega.TextureBuffer(...) | Graphics;
 * auto sfx  = vega.read_audio("x.wav") | Audio;
 * @endcode
 */
extern MAYAFLUX_API Creator vega;

} // namespace MayaFlux
