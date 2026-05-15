#include "Inspector.hpp"

#include "MayaFlux/Nodes/Node.hpp"
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
        Layer& layer,
        Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h, int depth)
    {
        const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;

        const std::string header_label = std::string(role_label(tree.role)) + ": "
            + Reflect::short_dynamic_type_name(*tree.node);

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

InspectResult Inspector::node(
    const std::shared_ptr<Nodes::Node>& n,
    Layer& layer,
    Context& context,
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

} // namespace MayaFlux::Portal::Forma
