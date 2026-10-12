#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(PIPEWIRE_BACKEND)

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>

#include "MayaFlux/Core/Backends/Audio/PipewireBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(2ms);
        }
        return predicate();
    }

    Core::GlobalStreamInfo output_config()
    {
        Core::GlobalStreamInfo config;
        config.sample_rate = 48000;
        config.buffer_size = 512;
        config.output.channels = 2;
        config.input.enabled = false;
        config.input.channels = 0;
        return config;
    }

    void set_silence_callback(Core::AudioStream& stream,
        uint32_t channels, std::shared_ptr<std::atomic<uint32_t>> callbacks)
    {
        stream.set_process_callback([channels, callbacks](void* output, void*, unsigned int frames) {
            if (output && frames > 0) {
                std::memset(output, 0,
                    static_cast<std::size_t>(frames) * channels * sizeof(double));
                callbacks->fetch_add(1, std::memory_order_release);
            }
            return 0;
        });
    }

    TEST(PipewireBackendTest, InitializesNativeObjectsAndReportsVersion)
    {
        Core::PipewireBackend backend;
        EXPECT_NE(backend.loop(), nullptr);
        EXPECT_NE(backend.context(), nullptr);
        EXPECT_NE(backend.core(), nullptr);
        EXPECT_FALSE(backend.get_version_string().empty());
    }

    TEST(PipewireBackendTest, EnumeratesOutputAndInputDevices)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);

        const auto outputs = devices->get_output_devices();
        const auto inputs = devices->get_input_devices();

        for (const auto& output : outputs) {
            EXPECT_FALSE(output.name.empty());
            EXPECT_GT(output.output_channels, 0U);
        }
        for (const auto& input : inputs) {
            EXPECT_FALSE(input.name.empty());
            EXPECT_GT(input.input_channels, 0U);
        }

        if (!outputs.empty()) {
            EXPECT_NE(devices->get_default_output_device(), 0U);
        }
        if (!inputs.empty()) {
            EXPECT_NE(devices->get_default_input_device(), 0U);
        }
    }

    TEST(PipewireBackendTest, ResolvesNamedDevicesAndFallsBackToDefaults)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        auto* pipewire_devices = dynamic_cast<Core::PipewireDevice*>(devices.get());
        ASSERT_NE(pipewire_devices, nullptr);

        const auto outputs = devices->get_output_devices();
        if (!outputs.empty()) {
            EXPECT_NE(pipewire_devices->find_output_node_id(outputs.front().name), 0U);
            EXPECT_EQ(pipewire_devices->find_output_node_id("missing-output-device"),
                devices->get_default_output_device());
        }

        const auto inputs = devices->get_input_devices();
        if (!inputs.empty()) {
            EXPECT_NE(pipewire_devices->find_input_node_id(inputs.front().name), 0U);
            EXPECT_EQ(pipewire_devices->find_input_node_id("missing-input-device"),
                devices->get_default_input_device());
        }
    }

    TEST(PipewireBackendTest, RunsOutputStreamAndDeliversCallbacks)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty()) {
            GTEST_SKIP() << "PipeWire has no output device";
        }

        auto config = output_config();

        auto stream = backend.create_stream(
            devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);

        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        set_silence_callback(*stream, config.output.channels, callbacks);

        stream->open();
        ASSERT_TRUE(stream->is_open());
        stream->start();
        const bool processed = eventually([&] {
            return callbacks->load(std::memory_order_acquire) >= 2;
        });
        EXPECT_TRUE(processed);
        EXPECT_TRUE(stream->is_running());
        stream->stop();
        EXPECT_FALSE(stream->is_running());
        stream->close();
        EXPECT_FALSE(stream->is_open());
    }

    TEST(PipewireBackendTest, OpensAndClosesWithoutStarting)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty()) {
            GTEST_SKIP() << "PipeWire has no output device";
        }

        auto config = output_config();
        auto stream = backend.create_stream(
            devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        EXPECT_FALSE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
        stream->open();
        EXPECT_TRUE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
        stream->close();
        EXPECT_FALSE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
    }

    TEST(PipewireBackendTest, RestartsAnOpenOutputStream)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty()) {
            GTEST_SKIP() << "PipeWire has no output device";
        }

        auto config = output_config();
        auto stream = backend.create_stream(
            devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        set_silence_callback(*stream, config.output.channels, callbacks);

        stream->open();
        stream->start();
        ASSERT_TRUE(eventually([&] { return callbacks->load(std::memory_order_acquire) >= 2; }));
        stream->stop();
        EXPECT_FALSE(stream->is_running());
        EXPECT_TRUE(stream->is_open());
        const auto before_restart = callbacks->load(std::memory_order_acquire);

        stream->start();
        EXPECT_TRUE(eventually([&] {
            return callbacks->load(std::memory_order_acquire) >= before_restart + 2;
        }));
        stream->stop();
        stream->close();
    }

    TEST(PipewireBackendTest, PausesAndResumesAnOutputStream)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty()) {
            GTEST_SKIP() << "PipeWire has no output device";
        }

        auto config = output_config();
        auto stream = backend.create_stream(
            devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        set_silence_callback(*stream, config.output.channels, callbacks);

        stream->open();
        stream->start();
        ASSERT_TRUE(eventually([&] { return callbacks->load(std::memory_order_acquire) >= 2; }));
        stream->pause();
        EXPECT_FALSE(stream->is_running());
        EXPECT_TRUE(stream->is_open());
        const auto before_resume = callbacks->load(std::memory_order_acquire);

        stream->resume();
        EXPECT_TRUE(eventually([&] {
            return callbacks->load(std::memory_order_acquire) >= before_resume + 2;
        }));
        stream->stop();
        stream->close();
    }

    TEST(PipewireBackendTest, DeliversInputToFullDuplexCallback)
    {
        Core::PipewireBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty() || devices->get_input_devices().empty()) {
            GTEST_SKIP() << "PipeWire needs output and input devices";
        }

        auto config = output_config();
        config.input.enabled = true;
        config.input.channels = 2;
        auto stream = backend.create_stream(
            devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);

        auto output_callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        auto input_callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        const auto channels = config.output.channels;
        stream->set_process_callback([output_callbacks, input_callbacks, channels](void* output, void* input, unsigned int frames) {
            if (output && frames > 0) {
                std::memset(output, 0,
                    static_cast<std::size_t>(frames) * channels * sizeof(double));
                output_callbacks->fetch_add(1, std::memory_order_release);
                if (input) {
                    input_callbacks->fetch_add(1, std::memory_order_release);
                }
            }
            return 0;
        });

        stream->open();
        stream->start();
        EXPECT_TRUE(eventually([&] {
            return input_callbacks->load(std::memory_order_acquire) > 0;
        }));
        EXPECT_GT(output_callbacks->load(std::memory_order_acquire), 0U);
        stream->stop();
        stream->close();
    }

    TEST(PipewireBackendTest, ReinitializesAfterBackendDestruction)
    {
        {
            Core::PipewireBackend backend;
            auto devices = backend.create_device_manager();
            ASSERT_NE(devices, nullptr);
        }

        Core::PipewireBackend restarted;
        EXPECT_NE(restarted.loop(), nullptr);
        auto devices = restarted.create_device_manager();
        EXPECT_NE(devices, nullptr);
    }

    TEST(PipewireBackendTest, KeepsSecondBackendUsableWhenFirstIsCleanedUp)
    {
        Core::PipewireBackend first_backend;
        auto first_devices = first_backend.create_device_manager();
        ASSERT_NE(first_devices, nullptr);

        Core::PipewireBackend second_backend;
        auto second_devices = second_backend.create_device_manager();
        ASSERT_NE(second_devices, nullptr);
        if (second_devices->get_output_devices().empty()) {
            GTEST_SKIP() << "PipeWire has no output device";
        }

        auto config = output_config();
        auto stream = second_backend.create_stream(
            second_devices->get_default_output_device(),
            second_devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);

        auto callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        set_silence_callback(*stream, config.output.channels, callbacks);

        stream->open();
        stream->start();
        ASSERT_TRUE(eventually([&] {
            return callbacks->load(std::memory_order_acquire) >= 2;
        }));

        first_devices.reset();
        first_backend.cleanup();

        EXPECT_FALSE(second_devices->get_output_devices().empty());
        const auto callbacks_before_check = callbacks->load(std::memory_order_acquire);
        EXPECT_TRUE(eventually([&] {
            return callbacks->load(std::memory_order_acquire) > callbacks_before_check;
        }));

        stream->stop();
        stream->close();
    }

    TEST(PipewireBackendTest, ReinitializesAfterExplicitCleanupAndDestruction)
    {
        std::unique_ptr<Core::PipewireBackend> backend;
        ASSERT_NO_THROW(backend = std::make_unique<Core::PipewireBackend>())
            << "initial backend construction";
        auto initial_devices = backend->create_device_manager();
        ASSERT_NE(initial_devices, nullptr);
        initial_devices.reset();
        backend->cleanup();
        backend.reset();

        std::unique_ptr<Core::PipewireBackend> restarted;
        ASSERT_NO_THROW(restarted = std::make_unique<Core::PipewireBackend>())
            << "backend construction after explicit cleanup";
        EXPECT_NE(restarted->loop(), nullptr);
        auto devices = restarted->create_device_manager();
        EXPECT_NE(devices, nullptr);
    }

}
}

#endif
