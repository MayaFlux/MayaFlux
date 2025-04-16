#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Feedback.hpp"
#include "MayaFlux/Buffers/NodeSource.hpp"
#include "MayaFlux/Core/BufferManager.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Tasks/Chain.hpp"

using namespace MayaFlux;
using namespace MayaFlux::Tasks;
using namespace MayaFlux::Nodes;
using namespace MayaFlux::Nodes::Generator;
using namespace MayaFlux::Nodes::Filters;

MayaFlux::Tasks::DAC& dac = MayaFlux::Tasks::DAC::instance();

int main()
{
    Init();

    auto sine = std::make_shared<Sine>(440.0f, 0.9);
    auto sine2 = std::make_shared<Sine>(880.0f, 0.5f);
    auto sine3 = std::make_shared<Sine>(440.0f, 0.5f);
    auto filtered_sine = std::make_shared<Sine>(220.0f, 0.5f);
    auto filter = std::make_shared<FIR>(filtered_sine, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });

    // sine >> dac;
    // sine2 >> Time(2.0);

    // auto timer = Timer(*MayaFlux::get_scheduler());
    // auto timer = TimedAction(*MayaFlux::get_scheduler());
    // timer.execute([sine]() { MayaFlux::add_node_to_root(sine); }, [sine]() { MayaFlux::remove_node_from_root(sine); }, 2);

    // auto node_player = NodeTimer(*MayaFlux::get_scheduler());
    // node_player.play_for(sine, 3);

    /*
        filter >> dac;
        auto filter2 = std::make_shared<FIR>(sine3, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });
        filter2 >> Time(3.0);

        EventChain()
            .then([&]() { sine >> dac; })
            .then([]() { std::cout << "Waiting\n"; }, 2.0)
            .then([&]() {
                MayaFlux::remove_node_from_root(sine);
                sine2 >> dac;
            },
                1.0)
            .then([&]() {
                MayaFlux::remove_node_from_root(sine2);
                std::cout << "Waiting again\n"; }, 3.0)
            .start();
    */
    /*
        (
            Sequence()
            >> Play(sine)
            >> Wait(2.0)
            >> Action([&]() {
                  remove_node_from_root(sine);
              })
            >> Play(sine2)
            >> Wait(1.5)
            >> Action([&]() {
                  sine2->set_frequency(880.0);
              })
            >> Wait(2.0)
            >> Action([&]() {
                  remove_node_from_root(sine2);
                  sine >> dac;
              })
            >> Play(filter)
            >> Wait(3.0))
            .execute();
    */
    auto noise = std::make_shared<Nodes::Generator::Stochastics::NoiseEngine>();
    // connect_node_to_channel(noise, 0);

    get_buffer_manager()->create_specialized_buffer<Buffers::NodeBuffer>(0, noise, true);

    auto feedback_processor = std::make_shared<Buffers::FeedbackProcessor>(0.99f);
    get_buffer_manager()->add_processor(feedback_processor, 0);

    add_processor([](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        for (size_t i = 0; i < buffer->get_num_samples(); i++) {
            // buffer->get_sample(i) *= get_uniform_random(-1.f, 1.f) * 0.25;
            buffer->get_sample(i) * 0.25;
        }
    },
        0);

    Start();
    std::cin.get();
    End();

    return 0;
}
