#include "TokenUnitManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void RootAudioUnit::resize_channels(uint32_t new_count, uint32_t new_buffer_size, ProcessingToken token)
{
    if (new_count <= channel_count)
        return;

    uint32_t old_count = channel_count;
    channel_count = new_count;
    buffer_size = new_buffer_size;

    root_buffers.resize(new_count);
    processing_chains.resize(new_count);

    for (uint32_t i = old_count; i < new_count; ++i) {
        root_buffers[i] = std::make_shared<RootAudioBuffer>(i, buffer_size);
        root_buffers[i]->initialize();
        root_buffers[i]->set_token_active(true);

        processing_chains[i] = std::make_shared<BufferProcessingChain>();
        processing_chains[i]->set_preferred_token(token);
        processing_chains[i]->set_enforcement_strategy(TokenEnforcementStrategy::FILTERED);

        root_buffers[i]->set_processing_chain(processing_chains[i]);
    }
}

void RootAudioUnit::resize_buffers(uint32_t new_buffer_size)
{
    buffer_size = new_buffer_size;
    for (auto& buffer : root_buffers) {
        buffer->resize(new_buffer_size);
    }
}

RootGraphicsUnit::RootGraphicsUnit()
    : root_buffer(std::make_shared<RootGraphicsBuffer>())
    , processing_chain(std::make_shared<BufferProcessingChain>())
{
}

void RootGraphicsUnit::initialize(ProcessingToken token)
{
    root_buffer->initialize();
    root_buffer->set_token_active(true);

    processing_chain->set_preferred_token(token);
    processing_chain->set_enforcement_strategy(TokenEnforcementStrategy::STRICT);

    root_buffer->set_processing_chain(processing_chain);
}

TokenUnitManager::TokenUnitManager(
    ProcessingToken default_audio_token,
    ProcessingToken default_graphics_token)
    : m_default_audio_token(default_audio_token)
    , m_default_graphics_token(default_graphics_token)
{
}

// ============================================================================
// Audio Unit Management
// ============================================================================

RootAudioUnit& TokenUnitManager::get_or_create_audio_unit(ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {
        std::lock_guard<std::mutex> lock(m_manager_mutex);
        auto [inserted_it, success] = m_audio_units.emplace(token, RootAudioUnit {});
        return inserted_it->second;
    }
    return it->second;
}

const RootAudioUnit& TokenUnitManager::get_audio_unit(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {
        error<std::out_of_range>(
            Journal::Component::Core,
            Journal::Context::BufferManagement,
            std::source_location::current(),
            "Audio unit not found for token {}",
            static_cast<int>(token));
    }
    return it->second;
}

RootAudioUnit& TokenUnitManager::get_audio_unit_mutable(ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {
        throw std::out_of_range("Audio unit not found for token");
    }
    return it->second;
}

RootAudioUnit& TokenUnitManager::ensure_and_get_audio_unit(ProcessingToken token, uint32_t channel)
{
    auto& unit = get_or_create_audio_unit(token);
    if (channel >= unit.channel_count) {
        ensure_audio_channels(token, channel + 1);
    }
    return unit;
}

bool TokenUnitManager::has_audio_unit(ProcessingToken token) const
{
    return m_audio_units.find(token) != m_audio_units.end();
}

std::vector<ProcessingToken> TokenUnitManager::get_active_audio_tokens() const
{
    std::vector<ProcessingToken> tokens;
    for (const auto& [token, unit] : m_audio_units) {
        if (!unit.root_buffers.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// ============================================================================
// Graphics Unit Management
// ============================================================================

RootGraphicsUnit& TokenUnitManager::get_or_create_graphics_unit(ProcessingToken token)
{
    auto it = m_graphics_units.find(token);
    if (it == m_graphics_units.end()) {
        std::lock_guard<std::mutex> lock(m_manager_mutex);

        auto [inserted_it, success] = m_graphics_units.emplace(token, RootGraphicsUnit {});
        inserted_it->second.initialize(token);

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Created new graphics unit for token {}", static_cast<int>(token));

        return inserted_it->second;
    }
    return it->second;
}

const RootGraphicsUnit& TokenUnitManager::get_graphics_unit(ProcessingToken token) const
{
    auto it = m_graphics_units.find(token);
    if (it == m_graphics_units.end()) {
        throw std::out_of_range("Graphics unit not found for token");
    }
    return it->second;
}

RootGraphicsUnit& TokenUnitManager::get_graphics_unit_mutable(ProcessingToken token)
{
    auto it = m_graphics_units.find(token);
    if (it == m_graphics_units.end()) {
        throw std::out_of_range("Graphics unit not found for token");
    }
    return it->second;
}

bool TokenUnitManager::has_graphics_unit(ProcessingToken token) const
{
    return m_graphics_units.find(token) != m_graphics_units.end();
}

std::vector<ProcessingToken> TokenUnitManager::get_active_graphics_tokens() const
{
    std::vector<ProcessingToken> tokens;
    for (const auto& [token, unit] : m_graphics_units) {
        if (unit.get_buffer() && !unit.get_buffer()->get_child_buffers().empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// ============================================================================
// Audio Unit Operations
// ============================================================================

void TokenUnitManager::ensure_audio_channels(
    ProcessingToken token,
    uint32_t channel_count)
{
    auto& unit = get_or_create_audio_unit(token);
    if (channel_count > unit.channel_count) {
        std::lock_guard<std::mutex> lock(m_manager_mutex);
        unit.resize_channels(channel_count, unit.buffer_size, token);
    }
}

void TokenUnitManager::resize_audio_buffers(ProcessingToken token, uint32_t buffer_size)
{
    auto& unit = get_or_create_audio_unit(token);
    std::lock_guard<std::mutex> lock(m_manager_mutex);
    unit.resize_buffers(buffer_size);
}

uint32_t TokenUnitManager::get_audio_channel_count(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    return (it != m_audio_units.end()) ? it->second.channel_count : 0;
}

uint32_t TokenUnitManager::get_audio_buffer_size(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    return (it != m_audio_units.end()) ? it->second.buffer_size : 512;
}
}
