#pragma once

#include "vulkan/vulkan.hpp"

namespace MayaFlux::Registry::Service {
struct DisplayService;
}

namespace MayaFlux::Core {

class VKContext;
class VKImage;
class VKSwapchain;
class VKCommandManager;
class Window;
class BackendResourceManager;

struct CaptureSlot {
    vk::Image image {};
    vk::DeviceMemory mem {};
    vk::Extent2D extent {};
    vk::CommandBuffer cmd {};
    vk::Fence fence {};
    std::atomic<bool> pending { false };
};

struct CaptureState {
#ifdef MAYAFLUX_PLATFORM_MACOS
    std::atomic<const std::vector<uint8_t>*> last_frame { nullptr };

    static constexpr size_t LAST_FRAME_MAX_READERS = 32;
    mutable std::array<std::atomic<const std::vector<uint8_t>*>, LAST_FRAME_MAX_READERS> last_frame_hazard_ptrs {};
    mutable std::array<std::atomic<bool>, LAST_FRAME_MAX_READERS> last_frame_slot_active { false };

    void retire_last_frame(const std::vector<uint8_t>* ptr);
#else
    std::atomic<std::shared_ptr<std::vector<uint8_t>>> last_frame;
#endif

    std::vector<std::unique_ptr<CaptureSlot>> slots;
    size_t slot_index {};
    vk::Format format {};
    uint32_t bpp {};
    std::thread readback_thread;
    std::atomic<bool> readback_running { false };

    using FrameObserver = std::function<void(
        const std::shared_ptr<std::vector<uint8_t>>&,
        uint32_t, uint32_t, uint32_t)>;
    using ObserverMap = std::unordered_map<uint32_t, FrameObserver>;

    std::atomic<uint32_t> next_observer_id { 1 };

#ifdef MAYAFLUX_PLATFORM_MACOS
    std::atomic<const ObserverMap*> observers { nullptr };

    static constexpr size_t OBSERVERS_MAX_READERS = 32;
    mutable std::array<std::atomic<const ObserverMap*>, OBSERVERS_MAX_READERS> observers_hazard_ptrs {};
    mutable std::array<std::atomic<bool>, OBSERVERS_MAX_READERS> observers_slot_active { false };

    void retire_observers(const ObserverMap* ptr);
#else
    std::atomic<std::shared_ptr<ObserverMap>> observers {
        std::make_shared<ObserverMap>()
    };
#endif

    ~CaptureState()
    {
        readback_running.store(false, std::memory_order_release);
        if (readback_thread.joinable())
            readback_thread.join();

#ifdef MAYAFLUX_PLATFORM_MACOS
        auto* old_frame = last_frame.exchange(nullptr);
        if (old_frame)
            retire_last_frame(old_frame);

        auto* old_obs = observers.exchange(nullptr);
        if (old_obs)
            retire_observers(old_obs);
#endif
    }
};

struct WindowRenderContext {
    std::shared_ptr<Window> window;
    vk::SurfaceKHR surface;
    std::unique_ptr<VKSwapchain> swapchain;

    std::vector<vk::Semaphore> image_available;
    std::vector<vk::Semaphore> render_finished;
    std::vector<vk::Fence> in_flight;

    std::vector<vk::CommandBuffer> clear_command_buffers;
    std::shared_ptr<VKImage> depth_image;

    bool needs_recreation {};
    size_t current_frame {};
    uint32_t current_image_index {};

    std::unique_ptr<CaptureState> capture;

    WindowRenderContext() = default;
    ~WindowRenderContext() = default;

    WindowRenderContext(WindowRenderContext&&) = default;
    WindowRenderContext& operator=(WindowRenderContext&&) = default;
    WindowRenderContext(const WindowRenderContext&) = delete;
    WindowRenderContext& operator=(const WindowRenderContext&) = delete;

    void cleanup(VKContext& context);
};

class MAYAFLUX_API BackendWindowHandler {
public:
    BackendWindowHandler(VKContext& context, VKCommandManager& command_manager);
    ~BackendWindowHandler() = default;

    void setup_backend_service(const std::shared_ptr<Registry::Service::DisplayService>& display_service);

    BackendWindowHandler(const BackendWindowHandler&) = delete;
    BackendWindowHandler& operator=(const BackendWindowHandler&) = delete;

    BackendWindowHandler(BackendWindowHandler&&) noexcept = default;
    BackendWindowHandler& operator=(BackendWindowHandler&&) noexcept = default;

    // ========================================================================
    // Window management
    // ========================================================================
    bool register_window(const std::shared_ptr<Window>& window);
    void unregister_window(const std::shared_ptr<Window>& window);
    [[nodiscard]] bool is_window_registered(const std::shared_ptr<Window>& window) const;

    // ========================================================================
    // Rendering
    // ========================================================================
    void render_window(const std::shared_ptr<Window>& window);
    void render_all_windows();
    void handle_window_resize();

    void submit_and_present(
        const std::shared_ptr<Window>& window,
        const vk::CommandBuffer& command_buffer);

    // ========================================================================
    // Access control
    // ========================================================================
    WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window);
    [[nodiscard]] const WindowRenderContext* find_window_context(const std::shared_ptr<Window>& window) const;

    // Information
    [[nodiscard]] uint32_t get_swapchain_image_count(const std::shared_ptr<Window>& window) const;

    // Cleanup
    void cleanup();

    void set_resource_manager(BackendResourceManager* resource_manager) { m_resource_manager = resource_manager; }

private:
    VKContext& m_context;
    VKCommandManager& m_command_manager;
    std::vector<WindowRenderContext> m_window_contexts;

    /**
     * @brief Create synchronization objects for a window's swapchain
     * @param config Window swapchain configuration to populate
     * @return True if creation succeeded
     */
    bool create_sync_objects(WindowRenderContext& config);

    /**
     * @brief Recreate the swapchain and related resources for a window
     * @param context Window render context
     */
    void recreate_swapchain_for_context(WindowRenderContext& context);

    /**
     * @brief Internal logic to recreate swapchain and related resources
     * @param context Window render context
     * @return True if recreation succeeded
     */
    bool recreate_swapchain_internal(WindowRenderContext& context);

    /**
     * @brief Ensure depth image exists at current swapchain extent
     * @param ctx Window context to create depth image for
     *
     * Creates or recreates a D32_SFLOAT depth image matching the
     * swapchain extent. No-op if depth image already matches.
     */
    void ensure_depth_image(WindowRenderContext& ctx);

    /**
     * @brief Render empty windows with clear color
     * @param ctx Window render context
     *
     * For windows that are registered for processing but have no buffers attached,
     * this performs a minimal clear pass so the window is visible and responsive
     * to input events.
     */
    void render_empty_window(WindowRenderContext& ctx);

    /**
     * @brief Ensure capture state is initialized for a window context
     * @param ctx Window render context to initialize capture state for
     *
     * If the context does not already have capture state and the associated window has
     * capture enabled, this initializes the capture state including allocating command buffers
     * and synchronization objects for readback.
     */
    void ensure_capture_state(WindowRenderContext& ctx);

    /**
     * @brief Capture the current frame from a window's swapchain
     * @param ctx Window render context to capture from
     *
     * This initiates an asynchronous readback of the current swapchain image into
     * CPU-accessible memory. The result is stored in the capture state for retrieval
     * via the display service.
     */
    void capture_frame(WindowRenderContext& ctx);

    /**
     * @brief Thread function to perform asynchronous readback of captured frames
     * @param state Capture state containing pending readbacks
     * @param dev Vulkan device to use for readback operations
     *
     * This thread continuously checks for pending capture slots and performs the necessary
     * Vulkan commands to read back the image data into CPU memory. The last captured frame
     * is stored atomically for retrieval by the display service.
     */
    void start_readback_thread(CaptureState& state, vk::Device dev);

    BackendResourceManager* m_resource_manager {};
};

}
