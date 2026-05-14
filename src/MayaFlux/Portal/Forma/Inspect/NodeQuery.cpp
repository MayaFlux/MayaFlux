#include "NodeQuery.hpp"

#include <cstddef>

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Portal/Forma/Forma.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Collapsible.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

namespace MayaFlux::Portal::Forma::Inspect {

namespace {

    constexpr float k_indent = 0.03F;

    struct RowResult {
        Collapsible collapsible;
        std::shared_ptr<Core::VKImage> text;
    };

    std::pair<uint32_t, uint32_t> row_pixel_dims(
        const std::shared_ptr<Core::Window>& window,
        float x_min, float x_max, float row_h)
    {
        const auto& ws = window->get_state();
        const auto w = static_cast<uint32_t>(
            (x_max - x_min) * 0.5F * static_cast<float>(ws.current_width));
        const auto h = static_cast<uint32_t>(
            row_h * 0.5F * static_cast<float>(ws.current_height));
        return { std::max(w, 1U), std::max(h, 1U) };
    }

    RowResult make_row(
        std::string_view label,
        Buffers::BufferManager& bm,
        Layer& layer,
        Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h,
        bool initially_open = true,
        glm::vec3 color_closed = glm::vec3(0.6F, 0.1F, 0.1F),
        glm::vec3 color_open = glm::vec3(0.8F, 0.2F, 0.2F))
    {
        auto [tw, th] = row_pixel_dims(window, x_min, x_max, row_h);

        std::shared_ptr<Core::VKImage> text_image;
        if (!label.empty()) {
            text_image = Portal::Text::press(
                label,
                { tw, th },
                Portal::Text::PressParams { .color = { 1.F, 1.F, 1.F, 1.F } });
        }

        const size_t buf_capacity = static_cast<const size_t>(12) * Kakshya::VertexLayout::for_meshes().stride_bytes;
        auto buf = std::make_shared<Buffers::FormaBuffer>(
            buf_capacity, Graphics::PrimitiveTopology::TRIANGLE_LIST);

        bm.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

        if (text_image) {
            buf->setup_rendering({
                .target_window = window,
                .additional_textures = { { "text", text_image } },
            });
        } else {
            buf->setup_rendering({ .target_window = window });
        }

        auto col = make_collapsible(buf, layer, get_bridge(), cursor,
            x_min, x_max, row_h, initially_open, color_closed, color_open);

        context.on_press(col.header_id, IO::MouseButtons::Left,
            [open = col.open, &layer, id = col.header_id](uint32_t, glm::vec2) {
                open->write(!open->value);
                layer.set_visible(id, true);
            });

        return RowResult { .collapsible = std::move(col), .text = text_image };
    }

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
        Buffers::BufferManager& bm,
        Layer& layer,
        Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h, int depth)
    {
        const float ind = x_min + static_cast<float>(depth) * k_indent;
        const std::string label = std::string(role_label(tree.role)) + ": "
            + Reflect::short_dynamic_type_name(*tree.node);

        auto [col, text] = make_row(
            label, bm, layer, context, window, cursor, ind, x_max, row_h, true);

        InspectResult result;
        result.collapsible = std::move(col);
        result.text = text;

        if (text) {
            auto node_ref = tree.node;
            auto text_ref = text;
            auto buf = result.collapsible.buf;
            auto staging = Buffers::create_image_staging_buffer(text_ref->get_size_bytes());

            result.links.emplace_back(
                [] { },
                [node_ref, text_ref, buf, staging]() mutable {
                    if (!buf)
                        return;
                    Portal::Text::repress(text_ref,
                        Reflect::short_dynamic_type_name(*node_ref)
                            + " " + std::to_string(node_ref->get_last_output()),
                        {},
                        staging);
                    buf->bind_texture(0, text_ref);
                });
        }

        for (const auto& child : tree.modulators) {
            auto child_result = inspect_modulator_tree(
                child, bm, layer, context, window, cursor, x_min, x_max, row_h, depth + 1);
            layer.relate(result.collapsible.header_id, child_result.collapsible.header_id);
            result.children.push_back(std::move(child_result));
        }

        return result;
    }

} // namespace

InspectResult node(
    Buffers::BufferManager& bm,
    const std::shared_ptr<Nodes::Node>& n,
    Layer& layer,
    Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    const float ind = x_min + static_cast<float>(depth) * k_indent;
    const std::string label = Reflect::short_dynamic_type_name(*n)
        + " " + std::to_string(n->get_last_output());

    auto [col, text] = make_row(
        label, bm, layer, context, window, cursor, ind, x_max, row_h, true);

    InspectResult result;
    result.collapsible = std::move(col);
    result.text = text;

    if (text) {
        auto node_ref = n;
        auto text_ref = text;
        auto buf = result.collapsible.buf;
        auto staging = Buffers::create_image_staging_buffer(text_ref->get_size_bytes());

        result.links.emplace_back(
            [] { },
            [node_ref, text_ref, buf, staging]() mutable {
                if (!buf)
                    return;
                Portal::Text::repress(text_ref,
                    Reflect::short_dynamic_type_name(*node_ref)
                        + " " + std::to_string(node_ref->get_last_output()),
                    {},
                    staging);
                buf->bind_texture(0, text_ref);
            });
    }

    for (const auto& tree : n->get_modulator_tree()) {
        auto child = inspect_modulator_tree(
            tree, bm, layer, context, window, cursor, x_min, x_max, row_h, depth + 1);
        layer.relate(result.collapsible.header_id, child.collapsible.header_id);
        result.children.push_back(std::move(child));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma::Inspect
