#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

std::pair<int, int> shift_parser(const std::string& str)
{
    size_t underscore = str.find('_');
    if (underscore == std::string::npos) {
        throw std::invalid_argument("Invalid format. Supply numberical format of nInputs_nOutpits like 25_2");
    }

    int inputs = std::stoi(str.substr(0, underscore));
    int outputs = std::stoi(str.substr(underscore + 1));
    return std::make_pair(inputs, outputs);
}

Filter::Filter(std::shared_ptr<Node> input, const std::string& zindex_shifts)
    : inputNode(input)
{
    shift_config = shift_parser(zindex_shifts);
    initialize_shift_buffers();
}

void Filter::initialize_shift_buffers()
{
    input_history.resize(shift_config.first + 1, 0.0f);
    output_history.resize(shift_config.second + 1, 0.0f);
}

}
