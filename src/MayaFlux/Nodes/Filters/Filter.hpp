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

    std::vector<double> coef_a;
    std::vector<double> coef_b;

public:
    Filter(std::shared_ptr<Node> input, const std::string& zindex_shifts);

    Filter(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef);

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

    void set_coefs(const std::vector<double>& new_coefs, Utils::coefficients type = Utils::coefficients::ALL);

    void update_coefs_from_node(int lenght, std::shared_ptr<Node> source, Utils::coefficients type = Utils::coefficients::ALL);

    void update_coef_from_input(int lenght, Utils::coefficients type = Utils::coefficients::ALL);

protected:
    void setACoefficients(const std::vector<double>& new_coefs);

    void setBCoefficients(const std::vector<double>& new_coefs);

    inline const std::vector<double>& getACoefficients() const { return coef_a; }
    inline const std::vector<double>& getBCoefficients() const { return coef_b; }

    virtual void initialize_shift_buffers();

    virtual double process_sample(double input) override = 0;

    virtual void update_inputs(double current_sample);

    virtual void update_outputs(double current_sample);
};

}
