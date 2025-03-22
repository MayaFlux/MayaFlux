#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

int main()
{
    MayaFlux::Core::Engine engine;
    MayaFlux::Core::GlobalStreamInfo stream_info;
    stream_info.buffer_size = 512;
    stream_info.sample_rate = 48000;
    stream_info.num_channels = 2;

    engine.Init(stream_info);

    MayaFlux::Nodes::NodeGraphManager graph_manager;
    std::shared_ptr<MayaFlux::Nodes::Generator::Sine> sine = std::make_shared<MayaFlux::Nodes::Generator::Sine>(0.5, 440.0, 0.0);

    graph_manager.add_to_root(sine);

    engine.Start();
    return 0;
}
