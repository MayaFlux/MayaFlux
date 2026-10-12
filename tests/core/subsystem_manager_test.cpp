#include "gtest/gtest.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Core/GlobalNetworkConfig.hpp"
#include "MayaFlux/Core/SubsystemManager.hpp"
#include "MayaFlux/Core/Subsystems/NetworkSubsystem.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::HookPosition;
    using Core::SubsystemManager;
    using Core::SubsystemTokens;
    using Core::SubsystemType;

    SubsystemTokens audio_tokens()
    {
        return {
            .Buffer = Buffers::ProcessingToken::AUDIO_BACKEND,
            .Node = Nodes::ProcessingToken::AUDIO_RATE,
            .Task = Vruta::ProcessingToken::SAMPLE_ACCURATE
        };
    }

    SubsystemTokens visual_tokens()
    {
        return {
            .Buffer = Buffers::ProcessingToken::GRAPHICS_BACKEND,
            .Node = Nodes::ProcessingToken::VISUAL_RATE,
            .Task = Vruta::ProcessingToken::FRAME_ACCURATE
        };
    }

    struct SubsystemCounters {
        int initialize { 0 };
        int register_callbacks { 0 };
        int start { 0 };
        int stop { 0 };
        int pause { 0 };
        int resume { 0 };
        int wait_until_running { 0 };
        int shutdown { 0 };
        Core::SubsystemProcessingHandle* handle { nullptr };
    };

    class FakeSubsystem : public Core::ISubsystem {
    public:
        FakeSubsystem(SubsystemType type, SubsystemTokens tokens,
            std::shared_ptr<SubsystemCounters> counters, bool becomes_ready = true)
            : m_type(type)
            , m_tokens(tokens)
            , m_counters(std::move(counters))
            , m_becomes_ready(becomes_ready)
        {
        }

        void register_callbacks() override { ++m_counters->register_callbacks; }

        void initialize(Core::SubsystemProcessingHandle& handle) override
        {
            ++m_counters->initialize;
            m_counters->handle = &handle;
            m_ready = m_becomes_ready;
        }

        void start() override
        {
            ++m_counters->start;
            m_running = true;
        }

        void stop() override
        {
            ++m_counters->stop;
            m_running = false;
        }

        void pause() override
        {
            ++m_counters->pause;
            m_running = false;
        }

        void resume() override
        {
            ++m_counters->resume;
            m_running = true;
        }

        void wait_until_running() override { ++m_counters->wait_until_running; }

        void shutdown() override
        {
            ++m_counters->shutdown;
            m_ready = false;
            m_running = false;
        }

        [[nodiscard]] SubsystemTokens get_tokens() const override { return m_tokens; }
        [[nodiscard]] bool is_ready() const override { return m_ready; }
        [[nodiscard]] bool is_running() const override { return m_running; }
        [[nodiscard]] SubsystemType get_type() const override { return m_type; }
        Core::SubsystemProcessingHandle* get_processing_context_handle() override { return m_counters->handle; }

    private:
        SubsystemType m_type;
        SubsystemTokens m_tokens;
        std::shared_ptr<SubsystemCounters> m_counters;
        bool m_becomes_ready;
        bool m_ready { false };
        bool m_running { false };
    };

    /**
     * @brief Owns a SubsystemManager together with the managers it distributes.
     */
    class ManagerRig {
    public:
        ManagerRig()
            : nodes(std::make_shared<Nodes::NodeGraphManager>())
            , buffers(std::make_shared<Buffers::BufferManager>())
            , scheduler(std::make_shared<Vruta::TaskScheduler>(48000))
            , manager(nodes, buffers, scheduler)
        {
        }

        ManagerRig(const ManagerRig&) = delete;
        ManagerRig& operator=(const ManagerRig&) = delete;
        ManagerRig(ManagerRig&&) = delete;
        ManagerRig& operator=(ManagerRig&&) = delete;

        ~ManagerRig() { manager.shutdown(); }

        std::shared_ptr<SubsystemCounters> add(SubsystemType type, SubsystemTokens tokens, bool becomes_ready = true)
        {
            auto counters = std::make_shared<SubsystemCounters>();
            manager.add_subsystem(type,
                std::make_shared<FakeSubsystem>(type, tokens, counters, becomes_ready));
            return counters;
        }

        std::shared_ptr<Nodes::NodeGraphManager> nodes;
        std::shared_ptr<Buffers::BufferManager> buffers;
        std::shared_ptr<Vruta::TaskScheduler> scheduler;
        SubsystemManager manager;
    };

    // ------------------------------------------------------------------
    // Registration
    // ------------------------------------------------------------------

    TEST(SubsystemManagerTest, NewManagerHoldsNoSubsystems)
    {
        ManagerRig rig;

        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::AUDIO));
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::GRAPHICS));
        EXPECT_EQ(rig.manager.get_subsystem(SubsystemType::AUDIO), nullptr);
        EXPECT_EQ(rig.manager.get_audio_subsystem(), nullptr);
        EXPECT_EQ(rig.manager.get_graphics_subsystem(), nullptr);
        EXPECT_EQ(rig.manager.get_input_subsystem(), nullptr);
        EXPECT_EQ(rig.manager.get_network_subsystem(), nullptr);
        EXPECT_TRUE(rig.manager.query_subsystem_status().empty());
    }

    TEST(SubsystemManagerTest, AddSubsystemInitializesAndRegistersCallbacksOnce)
    {
        ManagerRig rig;

        auto counters = rig.add(SubsystemType::CUSTOM, audio_tokens());

        EXPECT_EQ(counters->initialize, 1);
        EXPECT_EQ(counters->register_callbacks, 1);
        EXPECT_EQ(counters->start, 0);
        EXPECT_TRUE(rig.manager.has_subsystem(SubsystemType::CUSTOM));
        EXPECT_NE(rig.manager.get_subsystem(SubsystemType::CUSTOM), nullptr);
    }

    TEST(SubsystemManagerTest, AddSubsystemHandsItAHandleCarryingItsOwnTokens)
    {
        ManagerRig rig;

        auto audio = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto visual = rig.add(SubsystemType::GRAPHICS, visual_tokens());

        ASSERT_NE(audio->handle, nullptr);
        ASSERT_NE(visual->handle, nullptr);
        EXPECT_NE(audio->handle, visual->handle);
        EXPECT_EQ(audio->handle->get_tokens().Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
        EXPECT_EQ(audio->handle->get_tokens().Node, Nodes::ProcessingToken::AUDIO_RATE);
        EXPECT_EQ(audio->handle->get_tokens().Task, Vruta::ProcessingToken::SAMPLE_ACCURATE);
        EXPECT_EQ(visual->handle->get_tokens().Buffer, Buffers::ProcessingToken::GRAPHICS_BACKEND);
        EXPECT_EQ(rig.manager.get_validated_handle(SubsystemType::AUDIO), audio->handle);
        EXPECT_EQ(rig.manager.get_validated_handle(SubsystemType::GRAPHICS), visual->handle);
    }

    TEST(SubsystemManagerTest, AddingTheSameTypeAgainReplacesTheRegisteredSubsystem)
    {
        ManagerRig rig;

        rig.add(SubsystemType::CUSTOM, audio_tokens());
        auto first = rig.manager.get_subsystem(SubsystemType::CUSTOM);
        auto second_counters = rig.add(SubsystemType::CUSTOM, visual_tokens());

        auto current = rig.manager.get_subsystem(SubsystemType::CUSTOM);
        EXPECT_NE(current, first);
        EXPECT_EQ(current->get_tokens().Buffer, Buffers::ProcessingToken::GRAPHICS_BACKEND);
        EXPECT_EQ(rig.manager.get_validated_handle(SubsystemType::CUSTOM), second_counters->handle);
        EXPECT_EQ(rig.manager.query_subsystem_status().size(), 1U);
    }

    TEST(SubsystemManagerTest, GetValidatedHandleIsNullForAnUnregisteredType)
    {
        ManagerRig rig;

        EXPECT_EQ(rig.manager.get_validated_handle(SubsystemType::GRAPHICS), nullptr);
    }

    TEST(SubsystemManagerTest, CreatingGraphicsWithoutAWindowManagerIsRefused)
    {
        ManagerRig rig;
        Core::GlobalGraphicsConfig config;

        EXPECT_THROW(rig.manager.create_graphics_subsystem(config), std::runtime_error);
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::GRAPHICS));
    }

    TEST(SubsystemManagerTest, CreatingInputWithoutAnInputManagerIsRefused)
    {
        ManagerRig rig;
        Core::GlobalInputConfig config;

        EXPECT_THROW(rig.manager.create_input_subsystem(config), std::runtime_error);
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::INPUT));
    }

    TEST(SubsystemManagerTest, CreateNetworkSubsystemRegistersItUnderTheNetworkType)
    {
        ManagerRig rig;

        Core::GlobalNetworkConfig config;

        rig.manager.create_network_subsystem(config);

        EXPECT_TRUE(rig.manager.has_subsystem(SubsystemType::NETWORK));
        auto network = rig.manager.get_network_subsystem();
        ASSERT_NE(network, nullptr);
        EXPECT_TRUE(network->is_ready());
        EXPECT_FALSE(network->is_running());
        EXPECT_EQ(rig.manager.get_audio_subsystem(), nullptr);
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(SubsystemManagerTest, StartAllStartsReadySubsystemsAndWaitsForThem)
    {
        ManagerRig rig;
        auto audio = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto visual = rig.add(SubsystemType::GRAPHICS, visual_tokens());

        rig.manager.start_all_subsystems();

        EXPECT_EQ(audio->start, 1);
        EXPECT_EQ(visual->start, 1);
        EXPECT_EQ(audio->wait_until_running, 1);
        EXPECT_EQ(visual->wait_until_running, 1);
    }

    TEST(SubsystemManagerTest, StartAllSkipsSubsystemsThatNeverBecameReady)
    {
        ManagerRig rig;
        auto ready = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto unready = rig.add(SubsystemType::GRAPHICS, visual_tokens(), false);

        rig.manager.start_all_subsystems();

        EXPECT_EQ(ready->start, 1);
        EXPECT_EQ(unready->start, 0);
        EXPECT_EQ(unready->wait_until_running, 0);
    }

    TEST(SubsystemManagerTest, StartAllWithNothingRegisteredIsHarmless)
    {
        ManagerRig rig;

        EXPECT_NO_THROW(rig.manager.start_all_subsystems());
        EXPECT_NO_THROW(rig.manager.pause_all_subsystems());
        EXPECT_NO_THROW(rig.manager.resume_all_subsystems());
        EXPECT_NO_THROW(rig.manager.stop());
        EXPECT_NO_THROW(rig.manager.shutdown());
        EXPECT_TRUE(rig.manager.query_subsystem_status().empty());
    }

    TEST(SubsystemManagerTest, PauseAllPausesOnlyRunningSubsystems)
    {
        ManagerRig rig;
        auto running = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto idle = rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.get_subsystem(SubsystemType::AUDIO)->start();

        rig.manager.pause_all_subsystems();

        EXPECT_EQ(running->pause, 1);
        EXPECT_EQ(idle->pause, 0);
    }

    TEST(SubsystemManagerTest, ResumeAllResumesEveryReadySubsystem)
    {
        ManagerRig rig;
        auto ready = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto unready = rig.add(SubsystemType::GRAPHICS, visual_tokens(), false);

        rig.manager.resume_all_subsystems();

        EXPECT_EQ(ready->resume, 1);
        EXPECT_EQ(unready->resume, 0);
    }

    TEST(SubsystemManagerTest, StopStopsOnlyRunningSubsystems)
    {
        ManagerRig rig;
        auto running = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto idle = rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.start_all_subsystems();
        rig.manager.get_subsystem(SubsystemType::GRAPHICS)->stop();

        rig.manager.stop();

        EXPECT_EQ(running->stop, 1);
        EXPECT_EQ(idle->stop, 1);
        EXPECT_FALSE(rig.manager.get_subsystem(SubsystemType::AUDIO)->is_running());
    }

    TEST(SubsystemManagerTest, StopOfAnIdleManagerCallsNothing)
    {
        ManagerRig rig;
        auto idle = rig.add(SubsystemType::AUDIO, audio_tokens());

        rig.manager.stop();

        EXPECT_EQ(idle->stop, 0);
    }

    TEST(SubsystemManagerTest, StatusReportsReadyAndRunningPerType)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens(), false);
        rig.manager.get_subsystem(SubsystemType::AUDIO)->start();

        auto status = rig.manager.query_subsystem_status();

        ASSERT_EQ(status.size(), 2U);
        EXPECT_EQ(status.at(SubsystemType::AUDIO), std::make_pair(true, true));
        EXPECT_EQ(status.at(SubsystemType::GRAPHICS), std::make_pair(false, false));
    }

    TEST(SubsystemManagerTest, RemoveSubsystemShutsItDownAndForgetsIt)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());

        rig.manager.remove_subsystem(SubsystemType::AUDIO);

        EXPECT_EQ(counters->shutdown, 1);
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::AUDIO));
        EXPECT_EQ(rig.manager.get_validated_handle(SubsystemType::AUDIO), nullptr);
        EXPECT_TRUE(rig.manager.has_subsystem(SubsystemType::GRAPHICS));
    }

    TEST(SubsystemManagerTest, RemovingAnAbsentSubsystemChangesNothing)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());

        EXPECT_NO_THROW(rig.manager.remove_subsystem(SubsystemType::GRAPHICS));

        EXPECT_TRUE(rig.manager.has_subsystem(SubsystemType::AUDIO));
        EXPECT_EQ(counters->shutdown, 0);
    }

    TEST(SubsystemManagerTest, RemoveSubsystemDropsItsCrossAccessPermissions)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);
        ASSERT_TRUE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());

        rig.manager.remove_subsystem(SubsystemType::GRAPHICS);
        rig.add(SubsystemType::GRAPHICS, visual_tokens());

        EXPECT_FALSE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());
    }

    TEST(SubsystemManagerTest, ShutdownShutsDownReadySubsystemsAndEmptiesTheManager)
    {
        ManagerRig rig;
        auto ready = rig.add(SubsystemType::AUDIO, audio_tokens());
        auto unready = rig.add(SubsystemType::GRAPHICS, visual_tokens(), false);
        rig.manager.start_all_subsystems();

        rig.manager.shutdown();

        EXPECT_EQ(ready->shutdown, 1);
        EXPECT_EQ(unready->shutdown, 0);
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::AUDIO));
        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::GRAPHICS));
        EXPECT_TRUE(rig.manager.query_subsystem_status().empty());
    }

    TEST(SubsystemManagerTest, ShutdownTwiceShutsEachSubsystemDownOnce)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());

        rig.manager.shutdown();
        EXPECT_NO_THROW(rig.manager.shutdown());

        EXPECT_EQ(counters->shutdown, 1);
    }

    // ------------------------------------------------------------------
    // Process hooks
    // ------------------------------------------------------------------

    TEST(SubsystemManagerTest, PostProcessIsTheDefaultHookPosition)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());

        rig.manager.register_process_hook(SubsystemType::AUDIO, "tap", [](unsigned int) { });

        EXPECT_TRUE(counters->handle->post_process_hooks.contains("tap"));
        EXPECT_FALSE(counters->handle->pre_process_hooks.contains("tap"));
        EXPECT_TRUE(rig.manager.has_process_hook(SubsystemType::AUDIO, "tap"));
    }

    TEST(SubsystemManagerTest, PreProcessHooksLandInThePreCollection)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());

        rig.manager.register_process_hook(SubsystemType::AUDIO, "gate", [](unsigned int) { }, HookPosition::PRE_PROCESS);

        EXPECT_TRUE(counters->handle->pre_process_hooks.contains("gate"));
        EXPECT_FALSE(counters->handle->post_process_hooks.contains("gate"));
        EXPECT_TRUE(rig.manager.has_process_hook(SubsystemType::AUDIO, "gate"));
    }

    TEST(SubsystemManagerTest, RegisteredHooksReceiveTheFrameCountTheHandleInvokesThemWith)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());
        std::vector<unsigned int> seen;

        rig.manager.register_process_hook(SubsystemType::AUDIO, "log",
            [&seen](unsigned int frames) { seen.push_back(frames); });
        counters->handle->post_process_hooks.at("log")(256);

        EXPECT_EQ(seen, std::vector<unsigned int> { 256 });
    }

    TEST(SubsystemManagerTest, RegisteringTheSameNameReplacesTheHook)
    {
        ManagerRig rig;
        auto counters = rig.add(SubsystemType::AUDIO, audio_tokens());
        int first = 0;
        int second = 0;

        rig.manager.register_process_hook(SubsystemType::AUDIO, "tap", [&first](unsigned int) { ++first; });
        rig.manager.register_process_hook(SubsystemType::AUDIO, "tap", [&second](unsigned int) { ++second; });
        counters->handle->post_process_hooks.at("tap")(1);

        EXPECT_EQ(counters->handle->post_process_hooks.size(), 1U);
        EXPECT_EQ(first, 0);
        EXPECT_EQ(second, 1);
    }

    TEST(SubsystemManagerTest, UnregisterRemovesFromEitherPosition)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.manager.register_process_hook(SubsystemType::AUDIO, "pre", [](unsigned int) { }, HookPosition::PRE_PROCESS);
        rig.manager.register_process_hook(SubsystemType::AUDIO, "post", [](unsigned int) { });

        rig.manager.unregister_process_hook(SubsystemType::AUDIO, "pre");
        rig.manager.unregister_process_hook(SubsystemType::AUDIO, "post");

        EXPECT_FALSE(rig.manager.has_process_hook(SubsystemType::AUDIO, "pre"));
        EXPECT_FALSE(rig.manager.has_process_hook(SubsystemType::AUDIO, "post"));
    }

    TEST(SubsystemManagerTest, UnregisteringAnUnknownHookChangesNothing)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.manager.register_process_hook(SubsystemType::AUDIO, "keep", [](unsigned int) { });

        EXPECT_NO_THROW(rig.manager.unregister_process_hook(SubsystemType::AUDIO, "missing"));

        EXPECT_TRUE(rig.manager.has_process_hook(SubsystemType::AUDIO, "keep"));
    }

    TEST(SubsystemManagerTest, EmptyHookIsRejected)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());

        rig.manager.register_process_hook(SubsystemType::AUDIO, "empty", Core::ProcessHook {});

        EXPECT_FALSE(rig.manager.has_process_hook(SubsystemType::AUDIO, "empty"));
    }

    TEST(SubsystemManagerTest, HooksForAnUnregisteredSubsystemAreIgnored)
    {
        ManagerRig rig;

        EXPECT_NO_THROW(rig.manager.register_process_hook(SubsystemType::GRAPHICS, "tap", [](unsigned int) { }));
        EXPECT_NO_THROW(rig.manager.unregister_process_hook(SubsystemType::GRAPHICS, "tap"));
        EXPECT_FALSE(rig.manager.has_process_hook(SubsystemType::GRAPHICS, "tap"));
    }

    TEST(SubsystemManagerTest, HooksAreScopedToTheirSubsystem)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());

        rig.manager.register_process_hook(SubsystemType::AUDIO, "tap", [](unsigned int) { });

        EXPECT_TRUE(rig.manager.has_process_hook(SubsystemType::AUDIO, "tap"));
        EXPECT_FALSE(rig.manager.has_process_hook(SubsystemType::GRAPHICS, "tap"));
    }

    // ------------------------------------------------------------------
    // Cross subsystem access
    // ------------------------------------------------------------------

    TEST(SubsystemManagerTest, CrossReadIsDeniedUntilPermissionIsGranted)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());

        EXPECT_FALSE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());

        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);

        EXPECT_TRUE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());
    }

    TEST(SubsystemManagerTest, CrossReadReturnsTheTargetsBufferForTheRequestedChannel)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);

        auto data = rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0);

        ASSERT_TRUE(data.has_value());
        EXPECT_EQ(data->size(), rig.buffers->get_buffer_size(Buffers::ProcessingToken::AUDIO_BACKEND));
        EXPECT_EQ(data->data(), rig.buffers->get_buffer_data(Buffers::ProcessingToken::AUDIO_BACKEND, 0).data());
    }

    TEST(SubsystemManagerTest, CrossAccessIsDirectional)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);

        EXPECT_FALSE(rig.manager.read_cross_subsystem_buffer(SubsystemType::AUDIO, SubsystemType::GRAPHICS, 0).has_value());
    }

    TEST(SubsystemManagerTest, CrossReadFromAnUnregisteredRequesterIsDenied)
    {
        ManagerRig rig;
        rig.add(SubsystemType::AUDIO, audio_tokens());
        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);

        EXPECT_FALSE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());
    }

    TEST(SubsystemManagerTest, CrossReadOfAnUnregisteredTargetIsDeniedAndRegistersNothing)
    {
        ManagerRig rig;
        rig.add(SubsystemType::GRAPHICS, visual_tokens());
        rig.manager.allow_cross_access(SubsystemType::GRAPHICS, SubsystemType::AUDIO);

        EXPECT_FALSE(rig.manager.read_cross_subsystem_buffer(SubsystemType::GRAPHICS, SubsystemType::AUDIO, 0).has_value());

        EXPECT_FALSE(rig.manager.has_subsystem(SubsystemType::AUDIO));
        EXPECT_EQ(rig.manager.query_subsystem_status().size(), 1U);
    }

    // ------------------------------------------------------------------
    // Combined tokens
    // ------------------------------------------------------------------

    TEST(SubsystemManagerTest, CombinedTokenOperationRunsOnceWithAHandleForThePrimaryTokens)
    {
        ManagerRig rig;
        int runs = 0;
        SubsystemTokens seen {};

        rig.manager.execute_with_combined_tokens(audio_tokens(), visual_tokens(),
            [&](Core::SubsystemProcessingHandle& handle) {
                ++runs;
                seen = handle.get_tokens();
            });

        EXPECT_EQ(runs, 1);
        EXPECT_EQ(seen.Buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
        EXPECT_EQ(seen.Node, Nodes::ProcessingToken::AUDIO_RATE);
        EXPECT_EQ(seen.Task, Vruta::ProcessingToken::SAMPLE_ACCURATE);
    }

    TEST(SubsystemManagerTest, CombinedTokenOperationRegistersNoSubsystem)
    {
        ManagerRig rig;

        rig.manager.execute_with_combined_tokens(audio_tokens(), visual_tokens(),
            [](Core::SubsystemProcessingHandle&) { });

        EXPECT_TRUE(rig.manager.query_subsystem_status().empty());
    }

} // namespace
} // namespace MayaFlux::Test
