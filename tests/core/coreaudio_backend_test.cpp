#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_MACOS) && defined(COREAUDIO_BACKEND)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>

#include "MayaFlux/Core/Backends/Audio/CoreAudioBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            std::this_thread::sleep_for(2ms);
        }
        return predicate();
    }

    Core::GlobalStreamInfo output_config(uint32_t channels = 2)
    {
        Core::GlobalStreamInfo config;
        config.sample_rate = 48000;
        config.buffer_size = 512;
        config.output.enabled = true;
        config.output.channels = channels;
        config.input.enabled = false;
        config.input.channels = 0;
        return config;
    }

    TEST(CoreAudioBackendTest, CreatesBackendAndReportsVersion)
    {
        Core::CoreAudioBackend backend;
        EXPECT_EQ(backend.get_version_string(), "CoreAudio");
        EXPECT_NE(backend.create_device_manager(), nullptr);
    }

    TEST(CoreAudioBackendTest, EnumeratesDevicesWithUsableIndicesAndDefaults)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        auto* coreaudio_devices = dynamic_cast<Core::CoreAudioDevice*>(devices.get());
        ASSERT_NE(coreaudio_devices, nullptr);

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
            const auto default_output = devices->get_default_output_device();
            ASSERT_LT(default_output, outputs.size());
            EXPECT_TRUE(outputs[default_output].is_default_output);
            for (std::size_t index = 0; index < outputs.size(); ++index)
                EXPECT_NE(coreaudio_devices->resolve_output_device(index), kAudioObjectUnknown);
            EXPECT_EQ(coreaudio_devices->resolve_output_device(UINT32_MAX),
                coreaudio_devices->resolve_output_device(default_output));
        }

        if (!inputs.empty()) {
            const auto default_input = devices->get_default_input_device();
            ASSERT_LT(default_input, inputs.size());
            EXPECT_TRUE(inputs[default_input].is_default_input);
            for (std::size_t index = 0; index < inputs.size(); ++index)
                EXPECT_NE(coreaudio_devices->resolve_input_device(index), kAudioObjectUnknown);
            EXPECT_EQ(coreaudio_devices->resolve_input_device(UINT32_MAX),
                coreaudio_devices->resolve_input_device(default_input));
        }
    }

    TEST(CoreAudioBackendTest, OpensDefaultOutputAndFallsBackForInvalidDeviceIndex)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty())
            GTEST_SKIP() << "CoreAudio has no output device";

        auto config = output_config();
        auto stream = backend.create_stream(UINT32_MAX,
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        EXPECT_FALSE(stream->is_open());
        EXPECT_FALSE(stream->is_running());

        ASSERT_NO_THROW(stream->open());
        EXPECT_TRUE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
        stream->close();
        EXPECT_FALSE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
    }

    TEST(CoreAudioBackendTest, StartRequiresOpenStream)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        if (devices->get_output_devices().empty())
            GTEST_SKIP() << "CoreAudio has no output device";

        auto config = output_config();
        auto stream = backend.create_stream(devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        EXPECT_THROW(stream->start(), std::runtime_error);
        EXPECT_FALSE(stream->is_open());
        EXPECT_FALSE(stream->is_running());
    }

    TEST(CoreAudioBackendTest, DeliversOutputCallbacksThatWriteSilence)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        const auto outputs = devices->get_output_devices();
        if (outputs.empty())
            GTEST_SKIP() << "CoreAudio has no output device";

        ASSERT_LT(devices->get_default_output_device(), outputs.size());
        const auto channels = std::min(outputs[devices->get_default_output_device()].output_channels, 2U);
        auto config = output_config(channels);
        auto stream = backend.create_stream(devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);

        auto callback_count = std::make_shared<std::atomic<uint32_t>>(0);
        auto invalid_buffer = std::make_shared<std::atomic<bool>>(false);
        stream->set_process_callback([callback_count, invalid_buffer, channels](void* output, void*, unsigned int frames) {
            if (!output || frames == 0) {
                invalid_buffer->store(true, std::memory_order_release);
                return 0;
            }
            std::memset(output, 0, static_cast<std::size_t>(frames) * channels * sizeof(double));
            callback_count->fetch_add(1, std::memory_order_release);
            return 0;
        });

        stream->open();
        stream->start();
        EXPECT_TRUE(eventually([&] { return callback_count->load(std::memory_order_acquire) >= 2; }));
        EXPECT_FALSE(invalid_buffer->load(std::memory_order_acquire));
        EXPECT_TRUE(stream->is_open());
        EXPECT_TRUE(stream->is_running());
        stream->stop();
        EXPECT_FALSE(stream->is_running());
        stream->close();
        EXPECT_FALSE(stream->is_open());
    }

    TEST(CoreAudioBackendTest, RestartsPausesAndResumesOutputStream)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        const auto outputs = devices->get_output_devices();
        if (outputs.empty())
            GTEST_SKIP() << "CoreAudio has no output device";

        ASSERT_LT(devices->get_default_output_device(), outputs.size());
        const auto channels = std::min(outputs[devices->get_default_output_device()].output_channels, 2U);
        auto config = output_config(channels);
        auto stream = backend.create_stream(devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        auto callback_count = std::make_shared<std::atomic<uint32_t>>(0);
        stream->set_process_callback([callback_count, channels](void* output, void*, unsigned int frames) {
            if (output && frames > 0) {
                std::memset(output, 0, static_cast<std::size_t>(frames) * channels * sizeof(double));
                callback_count->fetch_add(1, std::memory_order_release);
            }
            return 0;
        });

        stream->open();
        stream->start();
        ASSERT_TRUE(eventually([&] { return callback_count->load(std::memory_order_acquire) >= 2; }));
        stream->stop();
        EXPECT_FALSE(stream->is_running());
        EXPECT_TRUE(stream->is_open());
        const auto before_restart = callback_count->load(std::memory_order_acquire);
        stream->start();
        ASSERT_TRUE(eventually([&] {
            return callback_count->load(std::memory_order_acquire) >= before_restart + 2;
        }));

        stream->pause();
        EXPECT_FALSE(stream->is_running());
        EXPECT_TRUE(stream->is_open());
        const auto before_resume = callback_count->load(std::memory_order_acquire);
        stream->resume();
        EXPECT_TRUE(eventually([&] {
            return callback_count->load(std::memory_order_acquire) >= before_resume + 2;
        }));
        EXPECT_TRUE(stream->is_running());
        stream->stop();
        stream->close();
    }

    TEST(CoreAudioBackendTest, DeliversFullDuplexCallbackWhenInputAndOutputExist)
    {
        Core::CoreAudioBackend backend;
        auto devices = backend.create_device_manager();
        ASSERT_NE(devices, nullptr);
        const auto outputs = devices->get_output_devices();
        const auto inputs = devices->get_input_devices();
        if (outputs.empty() || inputs.empty())
            GTEST_SKIP() << "CoreAudio full duplex requires both output and input devices";

        ASSERT_LT(devices->get_default_output_device(), outputs.size());
        ASSERT_LT(devices->get_default_input_device(), inputs.size());
        const auto output_channels = std::min(outputs[devices->get_default_output_device()].output_channels, 2U);
        const auto input_channels = std::min(inputs[devices->get_default_input_device()].input_channels, 2U);
        auto config = output_config(output_channels);
        config.input.enabled = true;
        config.input.channels = input_channels;
        auto stream = backend.create_stream(devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);

        auto output_callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        auto input_callbacks = std::make_shared<std::atomic<uint32_t>>(0);
        stream->set_process_callback([output_callbacks, input_callbacks, output_channels](void* output, void* input, unsigned int frames) {
            if (output && frames > 0) {
                std::memset(output, 0, static_cast<std::size_t>(frames) * output_channels * sizeof(double));
                output_callbacks->fetch_add(1, std::memory_order_release);
                if (input)
                    input_callbacks->fetch_add(1, std::memory_order_release);
            }
            return 0;
        });

        stream->open();
        stream->start();
        EXPECT_TRUE(eventually([&] { return input_callbacks->load(std::memory_order_acquire) > 0; }));
        EXPECT_GT(output_callbacks->load(std::memory_order_acquire), 0U);
        stream->stop();
        stream->close();
    }

    TEST(CoreAudioBackendTest, RepeatedCleanupAndDestructionAllowReinitialization)
    {
        {
            Core::CoreAudioBackend backend;
            ASSERT_NE(backend.create_device_manager(), nullptr);
            EXPECT_NO_THROW(backend.cleanup());
            EXPECT_NO_THROW(backend.cleanup());
        }

        Core::CoreAudioBackend restarted;
        EXPECT_EQ(restarted.get_version_string(), "CoreAudio");
        auto devices = restarted.create_device_manager();
        ASSERT_NE(devices, nullptr);
        const auto outputs = devices->get_output_devices();
        if (outputs.empty())
            GTEST_SKIP() << "CoreAudio has no output device to exercise after reinitialization";

        ASSERT_LT(devices->get_default_output_device(), outputs.size());
        const auto output_channels = std::min(outputs[devices->get_default_output_device()].output_channels, 2U);
        auto config = output_config(output_channels);
        auto stream = restarted.create_stream(devices->get_default_output_device(),
            devices->get_default_input_device(), config, nullptr);
        ASSERT_NE(stream, nullptr);
        auto callback_count = std::make_shared<std::atomic<uint32_t>>(0);
        stream->set_process_callback([callback_count, output_channels](void* output, void*, unsigned int frames) {
            if (output && frames > 0) {
                std::memset(output, 0,
                    static_cast<std::size_t>(frames) * output_channels * sizeof(double));
                callback_count->fetch_add(1, std::memory_order_release);
            }
            return 0;
        });

        stream->open();
        stream->start();
        EXPECT_TRUE(eventually([&] { return callback_count->load(std::memory_order_acquire) > 0; }));
        stream->stop();
        stream->close();
    }

}
}

#endif
