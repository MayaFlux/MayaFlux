#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
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

    auto sine = std::make_shared<Sine>(440.0f, 0.5);
    auto sine2 = std::make_shared<Sine>(880.0f, 0.5f);
    auto sine3 = std::make_shared<Sine>(440.0f, 0.5f);

    // auto timer = Timer(*MayaFlux::get_scheduler());
    // auto timer = TimedAction(*MayaFlux::get_scheduler());
    // timer.execute([sine]() { MayaFlux::add_node_to_root(sine); }, [sine]() { MayaFlux::remove_node_from_root(sine); }, 2);

    // auto node_player = NodeTimer(*MayaFlux::get_scheduler());
    // node_player.play_for(sine, 3);

    // sine >> dac;

    // sine2 >> Time(2.0);

    auto filtered_sine = std::make_shared<Sine>(220.0f, 0.5f);
    auto filter = std::make_shared<FIR>(filtered_sine, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });
    // filter >> dac;

    // auto filter2 = std::make_shared<FIR>(sine3, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });
    // filter2 >> Time(3.0);

    // EventChain()
    //     .then([&]() { sine >> dac; })
    //     .then([]() { std::cout << "Waiting\n"; }, 2.0)
    //     .then([&]() {
    //         MayaFlux::remove_node_from_root(sine);
    //         sine2 >> dac;
    //     },
    //         1.0)
    //     .then([&]() {
    //         MayaFlux::remove_node_from_root(sine2);
    //         std::cout << "Waiting again\n"; }, 3.0)
    //     .start();

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

    Start();
    std::cin.get();
    End();

    return 0;
}
