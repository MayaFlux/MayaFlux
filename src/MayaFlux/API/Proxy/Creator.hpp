#pragma once

#include "Domain.hpp"
#include "Evolve.hpp"
#include "Mint.hpp"
#include "Registry.hpp"

#include "MayaFlux/API/Depot.hpp"

namespace MayaFlux {

namespace Core {
    class VKImage;
}

namespace IO {
    using TextureResolver = std::function<std::shared_ptr<Core::VKImage>(const std::string& path)>;
    struct CameraConfig;
}

namespace Buffers {
    class VideoContainerBuffer;
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
    /**
     * @brief Sets a system going from a config: state that persists, a rule
     *        that advances it every cycle, and rendering attached.
     *
     * Overloaded on the config it is given: RelaxationGridBuffer::GridConfig
     * (optionally with a seed callable), VolumeGridBuffer::FlowConfig over a
     * volume, or SpatialFieldConfig over a field-compatible network. Each call
     * performs the whole sequence and returns the real object, which then
     * advances on its own each cycle. The call sets it going and does not step
     * it. Every call requires RenderConfig::target_window.
     *
     * @code
     * auto life  = vega.evolve(grid_config, seed, { .target_window = window });
     * auto smoke = vega.evolve(volume, flow_config, { .target_window = window });
     * @endcode
     *
     * @see Evolve
     */
    Evolve evolve;

    /**
     * @brief Builds a visible structure from a StructureConfig: resolves its
     *        source, constructs and registers what it needs, and attaches
     *        rendering.
     *
     * Overloaded on the arrangement: StructureConfig::Object, Model, Instances,
     * Assembly or Isosurface. Each call returns the real buffer. For Object,
     * Model and Instances the node or network is reached through the buffer's
     * get_node or get_network. Mint builds form and does not advance state,
     * which is what evolve is for. Every config requires
     * RenderConfig::target_window.
     *
     * @code
     * auto form = vega.mint(StructureConfig::Object {
     *     .mesh = mesh,
     *     .render = { .target_window = window } });
     * @endcode
     *
     * @see Mint
     * @see StructureConfig
     */
    Mint mint;

#define N(method_name, full_type_name)                                            \
    template <typename... Args>                                                   \
        requires std::constructible_from<full_type_name, Args...>                 \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>           \
    {                                                                             \
        auto obj = std::make_shared<full_type_name>(std::forward<Args>(args)...); \
        MF_LIVE_EXPOSE_NAMED(#method_name, obj);                                  \
        return obj;                                                               \
    }
    ALL_NODE_REGISTRATIONS
#undef N

#define W(method_name, full_type_name)                                            \
    template <typename... Args>                                                   \
        requires std::constructible_from<full_type_name, Args...>                 \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>           \
    {                                                                             \
        auto obj = std::make_shared<full_type_name>(std::forward<Args>(args)...); \
        MF_LIVE_EXPOSE_NAMED(#method_name, obj);                                  \
        return obj;                                                               \
    }
    ALL_NODE_NETWORK_REGISTRATIONS
#undef W

#define B(method_name, full_type_name)                                            \
    template <typename... Args>                                                   \
        requires std::constructible_from<full_type_name, Args...>                 \
    auto method_name(Args&&... args) -> std::shared_ptr<full_type_name>           \
    {                                                                             \
        auto obj = std::make_shared<full_type_name>(std::forward<Args>(args)...); \
        MF_LIVE_EXPOSE_NAMED(#method_name, obj);                                  \
        return obj;                                                               \
    }
    ALL_BUFFER_REGISTRATION
#undef B

    auto read_audio(const std::string& filepath) -> std::shared_ptr<Kakshya::SoundFileContainer>
    {
        auto container = load_sound_container(filepath);
        MF_LIVE_EXPOSE_AUTO(container);
        return container;
    }

    auto read_audio() -> std::shared_ptr<Kakshya::SoundFileContainer>
    {
        auto container = choose_audio();
        if (container)
            MF_LIVE_EXPOSE_AUTO(container);
        return container;
    }

    auto read_image(const std::string& filepath) -> std::shared_ptr<Buffers::TextureBuffer>
    {
        auto buffer = load_image_buffer(filepath);
        MF_LIVE_EXPOSE_AUTO(buffer);
        return buffer;
    }

    auto read_image() -> std::shared_ptr<Buffers::TextureBuffer>
    {
        auto buffer = choose_image();
        if (buffer)
            MF_LIVE_EXPOSE_AUTO(buffer);
        return buffer;
    }

    auto read_volume(const std::string& filepath) -> std::shared_ptr<Buffers::VolumeGridBuffer>
    {
        auto buffer = load_volume_buffer(filepath);
        MF_LIVE_EXPOSE_AUTO(buffer);
        return buffer;
    }

    auto read_volume() -> std::shared_ptr<Buffers::VolumeGridBuffer>
    {
        auto buffer = choose_volume();
        if (buffer)
            MF_LIVE_EXPOSE_AUTO(buffer);
        return buffer;
    }

    auto read_mesh(const std::string& filepath) -> MeshGroupHandle
    {
        return MeshGroupHandle(load_mesh_buffers(filepath));
    }

    auto read_mesh() -> MeshGroupHandle
    {
        return MeshGroupHandle(choose_mesh());
    }

    auto read_mesh_network(
        const std::string& filepath,
        IO::TextureResolver resolver = nullptr)
        -> std::shared_ptr<Nodes::Network::MeshNetwork>
    {
        auto network = load_mesh_network(filepath, std::move(resolver));
        MF_LIVE_EXPOSE_AUTO(network);
        return network;
    }

    auto read_mesh_network() -> std::shared_ptr<Nodes::Network::MeshNetwork>
    {
        auto network = choose_mesh_network();
        if (network)
            MF_LIVE_EXPOSE_AUTO(network);
        return network;
    }

    // ═══════════════════════════════════════════════════════════════
    // Camera (Special - defined in Creator.cpp)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Open a camera device and register it as a pipeable live source.
     *
     * Opens via IOManager::open_camera(), hooks the resulting CameraContainer
     * to a VideoContainerBuffer via IOManager::hook_camera_to_buffer(), and
     * returns the buffer — already pipeable through the generic VKBuffer
     * path (register_buffer), the same way read_image() returns an
     * already-pipeable TextureBuffer instead of a raw container.
     *
     * There is no dialog-backed no-argument overload: a camera is a device
     * to configure, not a file to browse to, the same reasoning behind
     * read_hid/read_midi/read_osc/read_tablet taking a config directly.
     *
     * @param config Device name, resolution hint, fps hint, format override.
     * @return Hooked VideoContainerBuffer, or nullptr on failure.
     */
    std::shared_ptr<Buffers::VideoContainerBuffer> read_camera(const IO::CameraConfig& config);

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
     * @brief Create and register tablet input node.
     * @param config Tablet input configuration.
     * @param binding Tablet input binding (use InputBinding::tablet(id)).
     * @return Shared pointer to TabletNode (already registered).
     */
    std::shared_ptr<Nodes::Input::TabletNode> read_tablet(
        const Nodes::Input::TabletConfig& config,
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
    std::shared_ptr<Buffers::VolumeGridBuffer> load_volume_buffer(const std::string& filepath);
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
 * Every call on vega is one of three kinds:
 * - Class-named factories (Sine, AudioBuffer, ModalNetwork, TextureBuffer and
 *   the rest of the registry) construct the class with exactly its
 *   constructor arguments. The pipe registers the result in a domain.
 * - read_* loaders take a source and return the real object. read_audio,
 *   read_image, read_mesh and read_mesh_network load from a path or, with no
 *   argument, from the matching choose_* call. read_mesh returns a
 *   MeshGroupHandle that the pipe registers. read_hid, read_midi, read_osc,
 *   read_tablet and read_input create an input node and register it against
 *   a binding.
 * - evolve and mint take a config and perform the whole sequence, returning
 *   the real object with rendering attached.
 *
 * @code
 * auto wave = vega.Sine(440.f) | Audio[0];
 * auto buf  = vega.AudioBuffer(0, 512) | Audio[0];
 * auto net  = vega.ModalNetwork(16, 220.0) | Audio[{0, 1}];
 * auto tex  = vega.TextureBuffer(...) | Graphics;
 *
 * auto sfx    = vega.read_audio("x.wav") | Audio;
 * auto image  = vega.read_image("x.png") | Graphics;
 * auto meshes = vega.read_mesh("x.fbx") | Graphics;
 * auto pad    = vega.read_midi(config, binding);
 *
 * auto life  = vega.evolve(grid_config, seed, { .target_window = window });
 * auto smoke = vega.evolve(volume, flow_config, { .target_window = window });
 * auto form  = vega.mint(StructureConfig::Object {
 *     .mesh = mesh,
 *     .render = { .target_window = window } });
 * @endcode
 */
extern MAYAFLUX_API Creator vega;

} // namespace MayaFlux
