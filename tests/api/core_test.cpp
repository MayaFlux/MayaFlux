#include "../test_config.h"

#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    constexpr auto k_end_deadline = 10s;

    struct StreamCase {
        const char* name;
        uint32_t sample_rate;
        uint32_t buffer_size;
        uint32_t channels;
    };

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

    /**
     * @brief Run @p work on its own thread and abort the process if it outlives the deadline.
     *
     * A deadlocked End() would otherwise hang the whole test binary, including its exit.
     */
    template <typename Work>
    void run_or_abort(Work work, const char* what)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto finished = done->get_future();
        std::thread([done, work = std::move(work)]() mutable {
            work();
            done->set_value();
        }).detach();

        if (finished.wait_for(k_end_deadline) != std::future_status::ready) {
            std::cerr << "core_test: " << what << " did not finish within the deadline, aborting\n"
                      << std::flush;
            std::_Exit(EXIT_FAILURE);
        }
    }

    class StartGate {
    public:
        void open() { m_open.store(true, std::memory_order_release); }

        void wait() const
        {
            while (!m_open.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }

    private:
        std::atomic<bool> m_open { false };
    };

    class ApiCoreTest : public ::testing::Test {
    protected:
        void SetUp() override { MayaFlux::End(); }
        void TearDown() override { MayaFlux::End(); }
    };

    class ApiCoreThreadsTest : public ::testing::TestWithParam<size_t> {
    protected:
        void SetUp() override { MayaFlux::End(); }
        void TearDown() override { MayaFlux::End(); }
    };

    class ApiCoreEngineTest : public ::testing::TestWithParam<StreamCase> {
    protected:
        void SetUp() override
        {
            MayaFlux::End();
            const auto& stream = GetParam();
            MayaFlux::Init(stream.sample_rate, stream.buffer_size, stream.channels, 0);
            MayaFlux::Start();
        }

        void TearDown() override { MayaFlux::End(); }
    };

} // namespace

TEST_F(ApiCoreTest, ContextIsTheSameEngineAcrossCalls)
{
    auto* first = &MayaFlux::get_context();
    auto* second = &MayaFlux::get_context();

    EXPECT_EQ(first, second);
    EXPECT_TRUE(MayaFlux::is_initialized());
}

TEST_F(ApiCoreTest, EndWithoutAnEngineIsHarmless)
{
    MayaFlux::End();
    MayaFlux::End();

    EXPECT_FALSE(MayaFlux::is_initialized());
}

TEST_F(ApiCoreTest, EndIsIdempotent)
{
    (void)MayaFlux::get_context();

    MayaFlux::End();
    MayaFlux::End();
    MayaFlux::End();

    EXPECT_FALSE(MayaFlux::is_initialized());
}

TEST_F(ApiCoreTest, AGetterAfterEndCreatesAFreshEngine)
{
    (void)MayaFlux::get_context();
    MayaFlux::End();
    ASSERT_FALSE(MayaFlux::is_initialized());

    (void)MayaFlux::get_context();

    EXPECT_TRUE(MayaFlux::is_initialized());
}

TEST_P(ApiCoreThreadsTest, ConcurrentFirstGettersAgreeOnOneEngine)
{
    const size_t thread_count = GetParam();
    constexpr int k_rounds = 50;

    for (int round = 0; round < k_rounds; ++round) {
        MayaFlux::End();

        StartGate gate;
        std::vector<Core::Engine*> seen(thread_count, nullptr);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&gate, &seen, i] {
                gate.wait();
                seen[i] = &MayaFlux::get_context();
            });
        }

        gate.open();
        for (auto& thread : threads) {
            thread.join();
        }

        auto* published = &MayaFlux::get_context();
        for (auto* engine : seen) {
            ASSERT_EQ(engine, published) << "round " << round;
        }
    }
}

TEST_P(ApiCoreThreadsTest, ConcurrentEndsAllReturnAndLeaveNoEngine)
{
    const size_t thread_count = GetParam();

    (void)MayaFlux::get_context();

    run_or_abort([thread_count] {
        StartGate gate;
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&gate] {
                gate.wait();
                MayaFlux::End();
            });
        }

        gate.open();
        for (auto& thread : threads) {
            thread.join();
        }
    },
        "concurrent End");

    EXPECT_FALSE(MayaFlux::is_initialized());
}

TEST_P(ApiCoreThreadsTest, GettersRacingEndNeverStallIt)
{
    const size_t thread_count = GetParam();

    (void)MayaFlux::get_context();

    auto stop = std::make_shared<std::atomic<bool>>(false);
    auto spins = std::make_shared<std::atomic<uint64_t>>(0);
    std::vector<std::thread> hammers;
    hammers.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        hammers.emplace_back([stop, spins] {
            while (!stop->load(std::memory_order_acquire)) {
                (void)&MayaFlux::get_context();
                spins->fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    ASSERT_TRUE(eventually([&] { return spins->load(std::memory_order_relaxed) > 1000; }));

    run_or_abort([] { MayaFlux::End(); }, "End with getters running on other threads");

    stop->store(true, std::memory_order_release);
    for (auto& hammer : hammers) {
        hammer.join();
    }
}

TEST_P(ApiCoreEngineTest, GlobalGetterOnTheAudioThreadDoesNotDeadlockEnd)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);
    sine->on_tick([calls](const Nodes::NodeContext&) {
        (void)MayaFlux::get_scheduler();
        calls->fetch_add(1, std::memory_order_relaxed);
    });
    MayaFlux::get_node_graph_manager()->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE);

    ASSERT_TRUE(eventually([&] { return calls->load(std::memory_order_relaxed) > 0; }))
        << "the audio thread never ran the callback";

    run_or_abort([] { MayaFlux::End(); }, "End with a global getter on the audio thread");

    EXPECT_FALSE(MayaFlux::is_initialized());
}

TEST_P(ApiCoreEngineTest, GlobalGettersOnEveryDomainThreadDoNotDeadlockEnd)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);
    sine->on_tick([calls](const Nodes::NodeContext&) {
        (void)MayaFlux::get_scheduler();
        (void)MayaFlux::get_node_graph_manager();
        (void)MayaFlux::get_buffer_manager();
        calls->fetch_add(1, std::memory_order_relaxed);
    });
    MayaFlux::get_node_graph_manager()->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE);

    ASSERT_TRUE(eventually([&] { return calls->load(std::memory_order_relaxed) > 0; }));

    run_or_abort([] { MayaFlux::End(); }, "End with several global getters on the audio thread");

    EXPECT_FALSE(MayaFlux::is_initialized());
}

INSTANTIATE_TEST_SUITE_P(ThreadCounts, ApiCoreThreadsTest,
    ::testing::Values(2U, 8U, 32U),
    [](const auto& info) { return "Threads" + std::to_string(info.param); });

INSTANTIATE_TEST_SUITE_P(StreamLayouts, ApiCoreEngineTest,
    ::testing::Values(
        StreamCase { "Stereo48k512", 48000, 512, 2 },
        StreamCase { "Mono44k256", 44100, 256, 1 },
        StreamCase { "Stereo48k64", 48000, 64, 2 }),
    [](const auto& info) { return info.param.name; });

} // namespace MayaFlux::Test
