#include "Inspector.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    std::string_view role_label(Nodes::ModulatorRole role)
    {
        switch (role) {
        case Nodes::ModulatorRole::FrequencyMod:
            return "freq";
        case Nodes::ModulatorRole::AmplitudeMod:
            return "amp";
        case Nodes::ModulatorRole::SignalMod:
            return "sig";
        case Nodes::ModulatorRole::Lhs:
            return "lhs";
        case Nodes::ModulatorRole::Rhs:
            return "rhs";
        case Nodes::ModulatorRole::ChainLink:
            return "->";
        default:
            return "mod";
        }
    }

    InspectResult inspect_modulator_tree(
        const Nodes::ModulatorTree& tree,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h, int depth)
    {
        const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;

        const std::string header_label = std::string(role_label(tree.role))
            + ": " + Reflect::short_dynamic_type_name(*tree.node);

        auto node_ref = tree.node;
        std::vector<ValueSpec> values {
            ValueSpec {
                .label = "out",
                .reader = [node_ref] { return std::to_string(node_ref->get_last_output()); },
            },
        };

        auto group = make_value_group(
            header_label, values,
            layer, context, window, cursor,
            ind, x_max, row_h, false);

        InspectResult result;
        result.group = std::move(group);

        for (const auto& child : tree.modulators) {
            auto child_result = inspect_modulator_tree(
                child, layer, context, window, cursor,
                x_min, x_max, row_h, depth + 1);
            layer.relate(
                result.group.header.header_id,
                child_result.group.header.header_id);
            result.children.push_back(std::move(child_result));
        }

        return result;
    }

} // namespace

// -----------------------------------------------------------------------------
// Single node
// -----------------------------------------------------------------------------

InspectResult Inspector::node(
    const std::shared_ptr<Nodes::Node>& n,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;
    const std::string header_label = Reflect::short_dynamic_type_name(*n);

    auto node_ref = n;
    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "out",
            .reader = [node_ref] { return std::to_string(node_ref->get_last_output()); },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);

    for (const auto& tree : n->get_modulator_tree()) {
        auto child = inspect_modulator_tree(
            tree, layer, context, window, cursor,
            x_min, x_max, row_h, depth + 1);
        layer.relate(
            result.group.header.header_id,
            child.group.header.header_id);
        result.children.push_back(std::move(child));
    }

    return result;
}

// -----------------------------------------------------------------------------
// Single network
// -----------------------------------------------------------------------------

InspectResult Inspector::node_network(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& net,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;

    const std::string header_label
        = std::string(Reflect::enum_to_string(net->get_topology()))
        + " / "
        + std::string(Reflect::enum_to_string(net->get_output_mode()));

    auto net_ref = net;
    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "nodes",
            .reader = [net_ref] { return std::to_string(net_ref->get_node_count()); },
        },
        ValueSpec {
            .label = "enabled",
            .reader = [net_ref] { return net_ref->is_enabled() ? "true" : "false"; },
        },
        ValueSpec {
            .label = "channels",
            .reader = [net_ref] {
                const auto ch = net_ref->get_registered_channels();
                std::string s;
                for (size_t i = 0; i < ch.size(); ++i) {
                    if (i)
                        s += ',';
                    s += std::to_string(ch[i]);
                }
                return s.empty() ? "-" : s;
            },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);
    return result;
}

// -----------------------------------------------------------------------------
// NodeGraphManager
// -----------------------------------------------------------------------------

InspectResult Inspector::node_graph_manager(
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const auto tokens = m_ngm.get_active_tokens();

    const std::string root_label = "NodeGraphManager ["
        + std::to_string(tokens.size()) + " token"
        + (tokens.size() == 1 ? "" : "s") + "]";

    auto& ngm = m_ngm;
    std::vector<ValueSpec> root_values {
        ValueSpec {
            .label = "tokens",
            .reader = [&ngm] { return std::to_string(ngm.get_active_tokens().size()); },
        },
    };

    auto root_group = make_value_group(
        root_label, root_values,
        layer, context, window, cursor,
        x_min, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(root_group);

    for (const auto tok : tokens) {
        const std::string tok_label = std::string(Reflect::enum_to_string(tok));
        const auto channels = m_ngm.get_all_channels(tok);

        std::vector<ValueSpec> tok_values {
            ValueSpec {
                .label = "nodes",
                .reader = [&ngm, tok] { return std::to_string(ngm.get_node_count(tok)); },
            },
            ValueSpec {
                .label = "networks",
                .reader = [&ngm, tok] { return std::to_string(ngm.get_network_count(tok)); },
            },
        };

        auto tok_group = make_value_group(
            tok_label, tok_values,
            layer, context, window, cursor,
            x_min + k_inspect_indent, x_max, row_h, false);

        InspectResult tok_result;
        tok_result.group = std::move(tok_group);

        layer.relate(result.group.header.header_id, tok_result.group.header.header_id);

        for (const auto ch : channels) {
            const std::string ch_label = "ch " + std::to_string(ch);
            const auto& nodes = m_ngm.get_nodes(tok, ch);

            std::vector<ValueSpec> ch_values {
                ValueSpec {
                    .label = "nodes",
                    .reader = [n = nodes.size()] { return std::to_string(n); },
                },
            };

            auto ch_group = make_value_group(
                ch_label, ch_values,
                layer, context, window, cursor,
                x_min + 2.F * k_inspect_indent, x_max, row_h, false);

            InspectResult ch_result;
            ch_result.group = std::move(ch_group);

            layer.relate(tok_result.group.header.header_id, ch_result.group.header.header_id);

            for (const auto& n : nodes) {
                if (!n)
                    continue;
                auto node_result = node(
                    n, layer, context, window, cursor,
                    x_min, x_max, row_h, 3);
                layer.relate(ch_result.group.header.header_id, node_result.group.header.header_id);
                ch_result.children.push_back(std::move(node_result));
            }

            for (const auto& net : m_ngm.get_networks(tok, ch)) {
                if (!net)
                    continue;
                auto net_result = node_network(
                    net, layer, context, window, cursor,
                    x_min, x_max, row_h, 2);
                layer.relate(tok_result.group.header.header_id, net_result.group.header.header_id);
                tok_result.children.push_back(std::move(net_result));
            }

            tok_result.children.push_back(std::move(ch_result));
        }

        result.children.push_back(std::move(tok_result));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma
