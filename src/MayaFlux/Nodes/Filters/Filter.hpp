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

    double gain = 1.0;
    bool bypass_enabled = false;

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

    void add_coef(int index, double value, Utils::coefficients type = Utils::coefficients::ALL);

    virtual void reset();

    inline void set_gain(double new_gain) { gain = new_gain; }

    inline double get_gain() const { return gain; }

    inline void set_bypass(bool enable) { bypass_enabled = enable; }

    inline bool is_bypass_enabled() const { return bypass_enabled; }

    inline int get_order() const { return std::max(coef_a.size() - 1, coef_b.size() - 1); }

    inline const std::vector<double>& get_input_history() const { return input_history; }

    inline const std::vector<double>& get_output_history() const { return output_history; }

    void normalize_coefficients(Utils::coefficients type = Utils::coefficients::ALL);

    std::complex<double> get_frequency_response(double frequency, double sample_rate) const;

    double get_magnitude_response(double frequency, double sample_rate) const;

    double get_phase_response(double frequency, double sample_rate) const;

    std::vector<double> processFull(unsigned int num_samples) override;

protected:
    void setACoefficients(const std::vector<double>& new_coefs);

    void setBCoefficients(const std::vector<double>& new_coefs);

    inline const std::vector<double>& getACoefficients() const { return coef_a; }
    inline const std::vector<double>& getBCoefficients() const { return coef_b; }

    void add_coef_internal(u_int64_t index, double value, std::vector<double>& buffer);

    virtual void initialize_shift_buffers();

    virtual double process_sample(double input) override = 0;

    virtual void update_inputs(double current_sample);

    virtual void update_outputs(double current_sample);
};

}
