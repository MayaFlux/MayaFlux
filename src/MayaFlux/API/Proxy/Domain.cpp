#include "Domain.hpp"

#include "MayaFlux/Buffers/BufferUtils.hpp"
#include "MayaFlux/Core/ProcessingArchitecture.hpp"

namespace MayaFlux {

Core::SubsystemTokens decompose_domain(Domain domain)
{
    return {
        .Buffer = static_cast<Buffers::ProcessingToken>((domain >> 16) & 0xFFFF),
        .Node = static_cast<Nodes::ProcessingToken>((domain >> 32) & 0xFFFF),
        .Task = static_cast<Vruta::ProcessingToken>(domain & 0xFFFF)
    };
}

Domain create_custom_domain(Nodes::ProcessingToken node_token,
    Buffers::ProcessingToken buffer_token,
    Vruta::ProcessingToken task_token)
{
    if (node_token == Nodes::ProcessingToken::AUDIO_RATE && (buffer_token & Buffers::ProcessingToken::FRAME_RATE)) {
        throw std::invalid_argument("AUDIO_RATE nodes incompatible with FRAME_RATE buffers");
    }

    if (node_token == Nodes::ProcessingToken::VISUAL_RATE && (buffer_token & Buffers::ProcessingToken::SAMPLE_RATE)) {
        throw std::invalid_argument("VISUAL_RATE nodes incompatible with SAMPLE_RATE buffers");
    }

    return compose_domain(node_token, buffer_token, task_token);
}

bool is_domain_valid(Domain domain)
{
    try {
        auto tokens = decompose_domain(domain);

        Buffers::validate_token(tokens.Buffer);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string domain_to_string(Domain domain)
{
    switch (domain) {
    case Domain::AUDIO:
        return "AUDIO";
    case Domain::AUDIO_PARALLEL:
        return "AUDIO_PARALLEL";
    case Domain::GRAPHICS:
        return "GRAPHICS";
    case Domain::GRAPHICS_ADAPTIVE:
        return "GRAPHICS_ADAPTIVE";
    case Domain::CUSTOM_ON_DEMAND:
        return "CUSTOM_ON_DEMAND";
    case Domain::CUSTOM_FLEXIBLE:
        return "CUSTOM_FLEXIBLE";
    case Domain::AUDIO_VISUAL_SYNC:
        return "AUDIO_VISUAL_SYNC";
    case Domain::AUDIO_GPU:
        return "AUDIO_GPU";
    default:
        return "UNKNOWN";
    }

    auto tokens = decompose_domain(domain);
    return "CUSTOM_DOMAIN(Node:" + std::to_string(static_cast<int>(tokens.Node)) + ",Buffer:" + std::to_string(static_cast<int>(tokens.Buffer)) + ",Task:" + std::to_string(static_cast<int>(tokens.Task)) + ")";
}

}
