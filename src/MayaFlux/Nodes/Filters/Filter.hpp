#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Filters {

std::pair<int, int> shift_parser(const std::string& str);

class Filter : public Node {
protected:
    std::shared_ptr<Node> inputNode;
    std::pair<int, int> shift_config;
    std::vector<double> input_history;
    std::vector<double> output_history;

public:
    Filter(std::shared_ptr<Node> input, const std::string& zindex_shifts);

    // Filter(std::vector<double> input, const std::string& zindex_shifts);

    virtual ~Filter() = default;

    inline int get_current_latency() const
    {
        return std::max(shift_config.first, shift_config.second);
    }

    inline std::pair<int, int> get_current_shift() const
    {
        return shift_config;
    }

    inline void set_shift(std::string& zindex_shifts)
    {
        shift_config = shift_parser(zindex_shifts);
        initialize_shift_buffers();
    }

protected:
    virtual void initialize_shift_buffers();

    virtual double processSample(double input) override = 0;
};

}
