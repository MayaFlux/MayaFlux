#pragma once

#include "MayaFlux/Utils.hpp"

namespace MayaFlux {

namespace Nodes {

    class Node {

    public:
        virtual ~Node() = default;
        virtual double process_sample(double input) = 0;
        virtual std::vector<double> processFull(unsigned int num_samples) = 0;

    private:
        MayaFlux::Utils::NodeProcessType node_type;
        std::string m_Name;
    };

    std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
    std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
    std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
}
}
