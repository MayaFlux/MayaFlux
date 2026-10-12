#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Input/InputAudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Core/ProcessingArchitecture.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#define REQUIRE_AUDIO(rig)                                                                    \
    do {                                                                                      \
        if (!(rig).bring_up())                                                                \
            GTEST_SKIP() << "Audio stream could not be created or started in this environment"; \
    } while (false)

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Registry::Service::AudioBackendService;

    constexpr uint32_t k_sample_rate = 48000;
    constexpr uint32_t k_buffer_size = 512;

    std::shared_ptr<Nodes::Node> constant_node(double value)
    {
        return std::make_shared<Nodes::Generator::Polynomial>([value](double) { return value; });
    }

    /**
     * @brief Owns an AudioSubsystem together with the managers behind its processing handle.
     *
     * bring_up() initializes and starts the real stream without registering the
     * stream callback, so the tests alone drive process_output, process_input and
     * process_audio.
     */
    class SubsystemRig {
    public:
        explicit SubsystemRig(uint32_t channels = 2, uint32_t input_channels = 2)
            : nodes(std::make_shared<Nodes::NodeGraphManager>())
            , buffers(std::make_shared<Buffers::BufferManager>(channels, input_channels, k_sample_rate, k_buffer_size))
            , tasks(std::make_shared<Vruta::TaskScheduler>(k_sample_rate))
            , channels(channels)
            , input_channels(input_channels)
        {
            stream_info.sample_rate = k_sample_rate;
            stream_info.buffer_size = k_buffer_size;
            stream_info.output.channels = channels;
            stream_info.input.enabled = false;
            stream_info.input.channels = input_channels;
        }

        SubsystemRig(const SubsystemRig&) = delete;
        SubsystemRig& operator=(const SubsystemRig&) = delete;
        SubsystemRig(SubsystemRig&&) = delete;
        SubsystemRig& operator=(SubsystemRig&&) = delete;

        ~SubsystemRig()
        {
            if (subsystem && subsystem->is_ready())
                subsystem->shutdown();
        }

        [[nodiscard]] bool construct()
        {
            try {
                subsystem = std::make_unique<Core::AudioSubsystem>(stream_info);
                handle = std::make_unique<Core::SubsystemProcessingHandle>(
                    buffers, nodes, tasks, nullptr, nullptr, subsystem->get_tokens());
            } catch (const std::exception&) {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool initialize_only()
        {
            if (!construct())
                return false;

            try {
                subsystem->initialize(*handle);
            } catch (const std::exception&) {
                return false;
            }
            return true;
        }

        [[nodiscard]] bool bring_up()
        {
            if (!initialize_only())
                return false;

            try {
                subsystem->start();
            } catch (const std::exception&) {
                return false;
            }
            return true;
        }

        void add_node(uint32_t channel, double value)
        {
            nodes->add_to_root(constant_node(value), Nodes::ProcessingToken::AUDIO_RATE, channel);
        }

        void add_buffer_node(uint32_t channel, double value)
        {
            buffers->create_audio_buffer<Buffers::NodeBuffer>(
                Buffers::ProcessingToken::AUDIO_BACKEND, channel, constant_node(value));
        }

        std::vector<double> render(uint32_t frames, int* status = nullptr)
        {
            std::vector<double> output(static_cast<size_t>(frames) * channels, 0.0);
            const int result = subsystem->process_output(output.data(), frames);
            if (status)
                *status = result;
            return output;
        }

        [[nodiscard]] AudioBackendService* service() const
        {
            return Registry::BackendRegistry::instance().get_service<AudioBackendService>();
        }

        std::shared_ptr<Nodes::NodeGraphManager> nodes;
        std::shared_ptr<Buffers::BufferManager> buffers;
        std::shared_ptr<Vruta::TaskScheduler> tasks;
        Core::GlobalStreamInfo stream_info;
        std::unique_ptr<Core::SubsystemProcessingHandle> handle;
        std::unique_ptr<Core::AudioSubsystem> subsystem;
        uint32_t channels;
        uint32_t input_channels;
    };

    class ObserverProbe {
    public:
        void record(const double* data, uint32_t size)
        {
            {
                std::lock_guard lock(m_mutex);
                ++m_calls;
                m_thread = std::this_thread::get_id();
                m_last.assign(data, data + size);
            }
            m_changed.notify_all();
        }

        bool wait_for_calls(int calls, std::chrono::milliseconds timeout = 2s)
        {
            std::unique_lock lock(m_mutex);
            return m_changed.wait_for(lock, timeout, [&] { return m_calls >= calls; });
        }

        [[nodiscard]] int calls() const
        {
            std::lock_guard lock(m_mutex);
            return m_calls;
        }

        [[nodiscard]] std::thread::id thread() const
        {
            std::lock_guard lock(m_mutex);
            return m_thread;
        }

        [[nodiscard]] std::vector<double> last() const
        {
            std::lock_guard lock(m_mutex);
            return m_last;
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        std::vector<double> m_last;
        std::thread::id m_thread;
        int m_calls { 0 };
    };

    uint32_t observe(AudioBackendService& service, const std::shared_ptr<ObserverProbe>& probe)
    {
        return service.register_output_observer(
            [probe](const double* data, uint32_t size) { probe->record(data, size); });
    }

    TEST(AudioSubsystemTest, ConstructionExposesConfigurationBackendAndTokens)
    {
        SubsystemRig rig(2);
        ASSERT_TRUE(rig.construct());
        const auto& subsystem = *rig.subsystem;

        EXPECT_FALSE(subsystem.is_ready());
        EXPECT_FALSE(subsystem.is_running());
        EXPECT_EQ(subsystem.get_type(), Core::SubsystemType::AUDIO);
        EXPECT_EQ(subsystem.get_stream_info().sample_rate, k_sample_rate);
        EXPECT_EQ(subsystem.get_stream_info().buffer_size, k_buffer_size);
        EXPECT_EQ(subsystem.get_stream_info().output.channels, 2U);
        EXPECT_NE(rig.subsystem->get_audio_backend(), nullptr);
        EXPECT_NE(subsystem.get_device_manager(), nullptr);
        EXPECT_EQ(subsystem.get_stream_manager(), nullptr);

        const auto tokens = subsystem.get_tokens();
        EXPECT_EQ(tokens.Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
        EXPECT_EQ(tokens.Node, Nodes::ProcessingToken::AUDIO_RATE);
        EXPECT_EQ(tokens.Task, Vruta::ProcessingToken::SAMPLE_ACCURATE);
    }

    TEST(AudioSubsystemTest, StartAndRegisterCallbacksThrowBeforeInitialize)
    {
        SubsystemRig rig;
        ASSERT_TRUE(rig.construct());

        EXPECT_THROW(rig.subsystem->start(), std::runtime_error);
        EXPECT_THROW(rig.subsystem->register_callbacks(), std::runtime_error);
        EXPECT_FALSE(rig.subsystem->is_running());
    }

    TEST(AudioSubsystemTest, StopPauseAndResumeAreNoOpsBeforeInitialize)
    {
        SubsystemRig rig;
        ASSERT_TRUE(rig.construct());

        EXPECT_NO_THROW(rig.subsystem->stop());
        EXPECT_NO_THROW(rig.subsystem->pause());
        EXPECT_NO_THROW(rig.subsystem->resume());
        EXPECT_FALSE(rig.subsystem->is_running());
        EXPECT_FALSE(rig.subsystem->is_ready());
    }

    TEST(AudioSubsystemTest, InitializeAndStartMakeSubsystemReadyAndRunning)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        EXPECT_TRUE(rig.subsystem->is_ready());
        EXPECT_TRUE(rig.subsystem->is_running());
        EXPECT_NE(rig.subsystem->get_stream_manager(), nullptr);
        EXPECT_EQ(rig.subsystem->get_processing_context_handle(), rig.handle.get());
        EXPECT_NO_THROW(rig.subsystem->wait_until_running());
    }

    TEST(AudioSubsystemTest, RegisterCallbacksSucceedsOnceInitialized)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        EXPECT_NO_THROW(rig.subsystem->register_callbacks());
        EXPECT_TRUE(rig.subsystem->is_running());
    }

    TEST(AudioSubsystemTest, PauseAndResumeKeepSubsystemRunning)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        EXPECT_NO_THROW(rig.subsystem->resume());
        EXPECT_NO_THROW(rig.subsystem->pause());
        EXPECT_TRUE(rig.subsystem->is_running());
        EXPECT_NO_THROW(rig.subsystem->resume());
        EXPECT_NO_THROW(rig.subsystem->resume());
        EXPECT_TRUE(rig.subsystem->is_running());
    }

    TEST(AudioSubsystemTest, StopIsIdempotentAndLeavesSubsystemReady)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        rig.subsystem->stop();
        EXPECT_FALSE(rig.subsystem->is_running());
        EXPECT_TRUE(rig.subsystem->is_ready());

        EXPECT_NO_THROW(rig.subsystem->stop());
        EXPECT_FALSE(rig.subsystem->is_running());
    }

    TEST(AudioSubsystemTest, ShutdownWithoutStopClearsReadinessAndReleasesBackendResources)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        rig.subsystem->shutdown();

        EXPECT_FALSE(rig.subsystem->is_ready());
        EXPECT_FALSE(rig.subsystem->is_running());
        EXPECT_EQ(rig.subsystem->get_stream_manager(), nullptr);
        EXPECT_EQ(rig.subsystem->get_device_manager(), nullptr);
        EXPECT_EQ(rig.subsystem->get_audio_backend(), nullptr);
    }

    TEST(AudioSubsystemTest, ShutdownAfterInitializeWithoutStartReleasesEverythingAndRemovesService)
    {
        SubsystemRig rig;
        if (!rig.initialize_only())
            GTEST_SKIP() << "Audio stream could not be created in this environment";

        EXPECT_TRUE(rig.subsystem->is_ready());
        EXPECT_FALSE(rig.subsystem->is_running());
        EXPECT_NE(rig.service(), nullptr);

        rig.subsystem->shutdown();

        EXPECT_FALSE(rig.subsystem->is_ready());
        EXPECT_EQ(rig.service(), nullptr);
        EXPECT_EQ(rig.subsystem->get_audio_backend(), nullptr);
    }

    TEST(AudioSubsystemTest, SubsystemsCanBeCreatedStartedAndShutDownRepeatedlyInOneProcess)
    {
        for (int round = 0; round < 3; ++round) {
            SubsystemRig rig;
            REQUIRE_AUDIO(rig);
            EXPECT_TRUE(rig.subsystem->is_running());
            rig.subsystem->shutdown();
            EXPECT_FALSE(rig.subsystem->is_ready());
        }
    }

    TEST(AudioSubsystemTest, StoppedSubsystemSilencesOutputAndInputAndReportsSuccess)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.5);
        rig.add_node(1, 0.5);
        rig.subsystem->stop();

        std::vector<double> output(64U * rig.channels, 7.0);
        EXPECT_EQ(rig.subsystem->process_output(output.data(), 64), 0);
        EXPECT_TRUE(std::ranges::all_of(output, [](double s) { return s == 0.0; }));

        std::vector<double> input(64U * rig.input_channels, 7.0);
        EXPECT_EQ(rig.subsystem->process_input(input.data(), 64), 0);
        EXPECT_TRUE(std::ranges::all_of(input, [](double s) { return s == 0.0; }));
    }

    TEST(AudioSubsystemTest, ProcessOutputRejectsNullBuffer)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        EXPECT_EQ(rig.subsystem->process_output(nullptr, 64), 1);
    }

    TEST(AudioSubsystemTest, UnderrunReturnsFailureButStillMixesNodes)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.25);
        rig.add_node(1, 0.25);

        int status = -1;
        const auto output = rig.render(k_buffer_size * 2, &status);

        EXPECT_EQ(status, 1);
        EXPECT_TRUE(std::ranges::all_of(output, [](double s) { return s == 0.25; }));
    }

    TEST(AudioSubsystemTest, ProcessInputDeliversDeinterleavedChannelsToInputBuffers)
    {
        SubsystemRig rig(2, 2);
        REQUIRE_AUDIO(rig);

        std::vector<double> input(static_cast<size_t>(k_buffer_size) * 2);
        for (size_t frame = 0; frame < k_buffer_size; ++frame) {
            input[frame * 2] = 0.001 * static_cast<double>(frame + 1);
            input[frame * 2 + 1] = -0.001 * static_cast<double>(frame + 1);
        }

        EXPECT_EQ(rig.subsystem->process_input(input.data(), k_buffer_size), 0);

        const auto left = rig.buffers->get_input_buffer(0);
        const auto right = rig.buffers->get_input_buffer(1);
        ASSERT_NE(left, nullptr);
        ASSERT_NE(right, nullptr);
        for (size_t frame = 0; frame < k_buffer_size; ++frame) {
            EXPECT_DOUBLE_EQ(left->get_data()[frame], input[frame * 2]);
            EXPECT_DOUBLE_EQ(right->get_data()[frame], input[frame * 2 + 1]);
        }
    }

    TEST(AudioSubsystemTest, ProcessAudioRunsHooksAroundInputAndOutputInKeyOrder)
    {
        SubsystemRig rig(2, 2);
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.5);
        rig.add_node(1, 0.5);

        std::vector<std::string> log;
        double output_seen_by_post_hook = 0.0;
        std::vector<double> output(64U * rig.channels, 0.0);

        rig.handle->pre_process_hooks["b_second"] = [&](unsigned int frames) {
            log.push_back("pre_b:" + std::to_string(frames));
        };
        rig.handle->pre_process_hooks["a_first"] = [&](unsigned int frames) {
            log.push_back("pre_a:" + std::to_string(frames));
        };
        rig.handle->post_process_hooks["post"] = [&](unsigned int frames) {
            log.push_back("post:" + std::to_string(frames));
            output_seen_by_post_hook = output[0];
        };

        std::vector<double> input(static_cast<size_t>(k_buffer_size) * 2, 0.0);
        EXPECT_EQ(rig.subsystem->process_audio(input.data(), output.data(), 64), 0);

        EXPECT_EQ(log, (std::vector<std::string> { "pre_a:64", "pre_b:64", "post:64" }));
        EXPECT_DOUBLE_EQ(output_seen_by_post_hook, 0.5);
    }

    struct MixCase {
        const char* name;
        uint32_t channels;
        uint32_t frames;
        double value;
    };

    class AudioSubsystemMixTest : public ::testing::TestWithParam<MixCase> { };

    TEST_P(AudioSubsystemMixTest, NodesFeedEveryChannelInterleavedAndClamped)
    {
        const auto& param = GetParam();
        SubsystemRig rig(param.channels, 0);
        REQUIRE_AUDIO(rig);
        for (uint32_t channel = 0; channel < param.channels; ++channel)
            rig.add_node(channel, param.value + 0.01 * channel);

        int status = -1;
        const auto output = rig.render(param.frames, &status);

        EXPECT_EQ(status, 0);
        ASSERT_EQ(output.size(), static_cast<size_t>(param.frames) * param.channels);
        for (uint32_t frame = 0; frame < param.frames; ++frame) {
            for (uint32_t channel = 0; channel < param.channels; ++channel) {
                const double expected = std::clamp(param.value + 0.01 * channel, -1.0, 1.0);
                EXPECT_NEAR(output[static_cast<size_t>(frame) * param.channels + channel], expected, 1e-12)
                    << "frame " << frame << " channel " << channel;
            }
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        AudioSubsystem,
        AudioSubsystemMixTest,
        ::testing::Values(
            MixCase { "MonoShortBlock", 1, 64, 0.25 },
            MixCase { "StereoMidBlock", 2, 128, 0.5 },
            MixCase { "StereoFullBuffer", 2, k_buffer_size, 0.75 },
            MixCase { "FourChannelsNegative", 4, 256, -0.125 },
            MixCase { "StereoClampedHigh", 2, 64, 2.0 },
            MixCase { "StereoClampedLow", 2, 64, -3.0 }),
        [](const ::testing::TestParamInfo<MixCase>& info) { return std::string(info.param.name); });

    struct SourceCase {
        const char* name;
        bool root_node;
        bool buffer_node;
    };

    class AudioSubsystemSourceTest : public ::testing::TestWithParam<SourceCase> { };

    TEST_P(AudioSubsystemSourceTest, OutputSumsBufferDataAndRootNodes)
    {
        const auto& param = GetParam();
        constexpr uint32_t frames = 128;
        SubsystemRig rig(2, 0);
        REQUIRE_AUDIO(rig);
        for (uint32_t channel = 0; channel < rig.channels; ++channel) {
            if (param.root_node)
                rig.add_node(channel, 0.25);
            if (param.buffer_node)
                rig.add_buffer_node(channel, 0.5);
        }

        int status = -1;
        const auto output = rig.render(frames, &status);
        EXPECT_EQ(status, 0);

        bool any_buffer_signal = false;
        for (uint32_t channel = 0; channel < rig.channels; ++channel) {
            const auto buffer_data = rig.handle->buffers.read_channel_data(channel);
            ASSERT_GE(buffer_data.size(), frames);
            for (uint32_t frame = 0; frame < frames; ++frame) {
                const double buffer_sample = buffer_data[frame];
                any_buffer_signal = any_buffer_signal || buffer_sample != 0.0;
                const double expected = std::clamp((param.root_node ? 0.25 : 0.0) + buffer_sample, -1.0, 1.0);
                EXPECT_NEAR(output[static_cast<size_t>(frame) * rig.channels + channel], expected, 1e-12)
                    << "frame " << frame << " channel " << channel;
            }
        }

        EXPECT_EQ(any_buffer_signal, param.buffer_node);
    }

    INSTANTIATE_TEST_SUITE_P(
        AudioSubsystem,
        AudioSubsystemSourceTest,
        ::testing::Values(
            SourceCase { "RootNodeOnly", true, false },
            SourceCase { "BufferOnly", false, true },
            SourceCase { "RootNodeAndBuffer", true, true },
            SourceCase { "Silent", false, false }),
        [](const ::testing::TestParamInfo<SourceCase>& info) { return std::string(info.param.name); });

    TEST(AudioSubsystemServiceTest, ServiceIsRegisteredWhileInitializedAndRemovedOnShutdown)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);

        EXPECT_NE(rig.service(), nullptr);
        EXPECT_TRUE(rig.service()->get_output_snapshot);
        EXPECT_TRUE(rig.service()->register_output_observer);
        EXPECT_TRUE(rig.service()->unregister_output_observer);

        rig.subsystem->shutdown();
        EXPECT_EQ(rig.service(), nullptr);
    }

    TEST(AudioSubsystemServiceTest, SnapshotIsEmptyUntilACycleThenMatchesTheRenderedOutput)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.25);
        rig.add_node(1, -0.5);

        EXPECT_TRUE(rig.service()->get_output_snapshot().empty());

        const auto output = rig.render(64);

        const auto snapshot = rig.service()->get_output_snapshot();
        ASSERT_EQ(snapshot.size(), output.size());
        EXPECT_TRUE(std::equal(snapshot.begin(), snapshot.end(), output.begin()));
    }

    class AudioSubsystemObserverTest : public ::testing::TestWithParam<int> { };

    TEST_P(AudioSubsystemObserverTest, EveryObserverReceivesTheCycleOnTheNotifyThread)
    {
        const int observer_count = GetParam();
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.25);
        rig.add_node(1, 0.25);

        std::vector<std::shared_ptr<ObserverProbe>> probes;
        std::vector<uint32_t> ids;
        for (int i = 0; i < observer_count; ++i) {
            probes.push_back(std::make_shared<ObserverProbe>());
            ids.push_back(observe(*rig.service(), probes.back()));
        }

        std::ranges::sort(ids);
        EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end());

        const auto output = rig.render(64);

        for (const auto& probe : probes) {
            ASSERT_TRUE(probe->wait_for_calls(1));
            EXPECT_EQ(probe->calls(), 1);
            EXPECT_NE(probe->thread(), std::this_thread::get_id());
            EXPECT_EQ(probe->last(), output);
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        AudioSubsystem,
        AudioSubsystemObserverTest,
        ::testing::Values(1, 2, 4),
        [](const ::testing::TestParamInfo<int>& info) { return "Observers" + std::to_string(info.param); });

    TEST(AudioSubsystemServiceTest, UnregisteredObserverStopsReceivingCycles)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.25);
        rig.add_node(1, 0.25);

        auto kept = std::make_shared<ObserverProbe>();
        auto removed = std::make_shared<ObserverProbe>();
        observe(*rig.service(), kept);
        const auto removed_id = observe(*rig.service(), removed);

        (void)rig.render(64);
        ASSERT_TRUE(kept->wait_for_calls(1));
        ASSERT_TRUE(removed->wait_for_calls(1));

        rig.service()->unregister_output_observer(removed_id);
        (void)rig.render(64);
        ASSERT_TRUE(kept->wait_for_calls(2));
        std::this_thread::sleep_for(100ms);

        EXPECT_EQ(removed->calls(), 1);
        EXPECT_EQ(kept->calls(), 2);
    }

    TEST(AudioSubsystemServiceTest, StopEndsNotificationsAndNoFurtherCallbacksArrive)
    {
        SubsystemRig rig;
        REQUIRE_AUDIO(rig);
        rig.add_node(0, 0.25);
        rig.add_node(1, 0.25);
        auto probe = std::make_shared<ObserverProbe>();
        observe(*rig.service(), probe);

        (void)rig.render(64);
        ASSERT_TRUE(probe->wait_for_calls(1));

        rig.subsystem->stop();
        const int calls_at_stop = probe->calls();
        std::vector<double> output(64U * rig.channels, 0.0);
        (void)rig.subsystem->process_output(output.data(), 64);
        std::this_thread::sleep_for(100ms);

        EXPECT_EQ(probe->calls(), calls_at_stop);
    }

}
}

#undef REQUIRE_AUDIO
