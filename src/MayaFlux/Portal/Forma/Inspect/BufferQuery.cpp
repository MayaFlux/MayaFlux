#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Input/InputAudioBuffer.hpp"
#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"
#include "MayaFlux/Buffers/Root/RootGraphicsBuffer.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"

namespace MayaFlux::Portal::Forma {

// -----------------------------------------------------------------------------
// Single buffer + processing chain
// -----------------------------------------------------------------------------

InspectResult Inspector::buffer(
    const std::shared_ptr<Buffers::Buffer>& buf,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;
    const std::string header_label = Reflect::short_dynamic_type_name(*buf);

    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "default_proc",
            .reader = [buf] {
                auto p = buf->get_default_processor();
                return p ? std::string(Reflect::short_dynamic_type_name(*p)) : "-";
            },
        },
    };

    auto chain = buf->get_processing_chain();
    if (chain) {
        auto pre = chain->get_preprocessor(buf);
        values.push_back(ValueSpec {
            .label = "pre",
            .reader = [pre] {
                return pre ? std::string(Reflect::short_dynamic_type_name(*pre)) : "-";
            },
        });

        const auto& processors = chain->get_processors(buf);
        for (size_t i = 0; i < processors.size(); ++i) {
            auto p = processors[i];
            values.push_back(ValueSpec {
                .label = "chain[" + std::to_string(i) + "]",
                .reader = [p] {
                    return p ? std::string(Reflect::short_dynamic_type_name(*p)) : "-";
                },
            });
        }

        auto post = chain->get_postprocessor(buf);
        values.push_back(ValueSpec {
            .label = "post",
            .reader = [post] {
                return post ? std::string(Reflect::short_dynamic_type_name(*post)) : "-";
            },
        });

        auto final_p = chain->get_final_processor(buf);
        values.push_back(ValueSpec {
            .label = "final",
            .reader = [final_p] {
                return final_p ? std::string(Reflect::short_dynamic_type_name(*final_p)) : "-";
            },
        });
    }

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);
    return result;
}

// -----------------------------------------------------------------------------
// Root audio buffer: single channel
// -----------------------------------------------------------------------------

InspectResult Inspector::root_audio_buffer(
    Buffers::ProcessingToken token, uint32_t channel,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    auto root = m_bm.get_root_audio_buffer(token, channel);
    const auto children = root->get_child_buffers();

    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;
    const std::string header_label = "ch " + std::to_string(channel);

    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "samples",
            .reader = [root] { return std::to_string(root->get_num_samples()); },
        },
        ValueSpec {
            .label = "children",
            .reader = [root] { return std::to_string(root->get_num_children()); },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);

    auto root_result = buffer(
        root, layer, context, window, cursor,
        x_min, x_max, row_h, depth + 1);
    layer.relate(result.group.header.header_id, root_result.group.header.header_id);
    result.children.push_back(std::move(root_result));

    for (const auto& child : children) {
        auto child_result = buffer(
            child, layer, context, window, cursor,
            x_min, x_max, row_h, depth + 2);
        layer.relate(result.group.header.header_id, child_result.group.header.header_id);
        result.children.push_back(std::move(child_result));
    }

    return result;
}

// -----------------------------------------------------------------------------
// Root audio buffer: all channels
// -----------------------------------------------------------------------------

InspectResult Inspector::root_audio_buffer(
    Buffers::ProcessingToken token,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    const uint32_t ch_count = m_bm.get_num_channels(token);

    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;
    const std::string header_label = std::string(Reflect::enum_to_string(token))
        + " [" + std::to_string(ch_count) + " ch]";

    auto& bm = m_bm;
    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "channels",
            .reader = [&bm, token] { return std::to_string(bm.get_num_channels(token)); },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);

    for (uint32_t ch = 0; ch < ch_count; ++ch) {
        auto ch_result = root_audio_buffer(
            token, ch,
            layer, context, window, cursor,
            x_min, x_max, row_h, depth + 1);
        layer.relate(result.group.header.header_id, ch_result.group.header.header_id);
        result.children.push_back(std::move(ch_result));
    }

    return result;
}

// -----------------------------------------------------------------------------
// Root graphics buffer
// -----------------------------------------------------------------------------

InspectResult Inspector::root_graphics_buffer(
    Buffers::ProcessingToken token,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h, int depth)
{
    auto root = m_bm.get_root_graphics_buffer(token);
    const auto children = root->get_child_buffers();

    const float ind = x_min + static_cast<float>(depth) * k_inspect_indent;
    const std::string header_label = "graphics ["
        + std::string(Reflect::enum_to_string(token)) + "]";

    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "vk_buffers",
            .reader = [root] { return std::to_string(root->get_buffer_count()); },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        ind, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);

    auto root_result = buffer(
        root, layer, context, window, cursor,
        x_min, x_max, row_h, depth + 1);
    layer.relate(result.group.header.header_id, root_result.group.header.header_id);
    result.children.push_back(std::move(root_result));

    for (const auto& child : children) {
        auto child_result = buffer(
            child, layer, context, window, cursor,
            x_min, x_max, row_h, depth + 2);
        layer.relate(result.group.header.header_id, child_result.group.header.header_id);
        result.children.push_back(std::move(child_result));
    }

    return result;
}

// -----------------------------------------------------------------------------
// BufferManager aggregate
// -----------------------------------------------------------------------------

InspectResult Inspector::buffer_manager(
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const auto tokens = m_bm.get_active_tokens();
    const uint32_t in_count = m_bm.get_num_input_channels();

    const std::string header_label = "BufferManager";

    auto& bm = m_bm;
    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "tokens",
            .reader = [&bm] { return std::to_string(bm.get_active_tokens().size()); },
        },
        ValueSpec {
            .label = "inputs",
            .reader = [&bm] { return std::to_string(bm.get_num_input_channels()); },
        },
    };

    auto group = make_value_group(
        header_label, values,
        layer, context, window, cursor,
        x_min, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);

    for (const auto tok : tokens) {
        const bool is_audio = tok == Buffers::ProcessingToken::AUDIO_BACKEND
            || tok == Buffers::ProcessingToken::AUDIO_PARALLEL;

        InspectResult tok_result = is_audio
            ? root_audio_buffer(tok, layer, context, window, cursor, x_min, x_max, row_h, 1)
            : root_graphics_buffer(tok, layer, context, window, cursor, x_min, x_max, row_h, 1);

        layer.relate(result.group.header.header_id, tok_result.group.header.header_id);
        result.children.push_back(std::move(tok_result));
    }

    if (in_count > 0) {
        const std::string in_label = "inputs [" + std::to_string(in_count) + "]";
        auto in_group = make_value_group(
            in_label, {},
            layer, context, window, cursor,
            x_min + k_inspect_indent, x_max, row_h, false);

        InspectResult in_result;
        in_result.group = std::move(in_group);
        layer.relate(result.group.header.header_id, in_result.group.header.header_id);

        for (uint32_t ch = 0; ch < in_count; ++ch) {
            auto buf = m_bm.get_input_buffer(ch);
            auto buf_result = buffer(
                std::dynamic_pointer_cast<Buffers::Buffer>(buf), layer, context, window, cursor,
                x_min, x_max, row_h, 2);
            layer.relate(in_result.group.header.header_id, buf_result.group.header.header_id);
            in_result.children.push_back(std::move(buf_result));
        }

        result.children.push_back(std::move(in_result));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma
