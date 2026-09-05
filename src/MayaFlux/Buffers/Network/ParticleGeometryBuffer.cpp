#include "ParticleGeometryBuffer.hpp"

#include "MutationClaimProcessor.hpp"
#include "PopulationProcessor.hpp"
#include "SpatialHashProcessor.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/VertexFieldProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/OperatorChain.hpp"
#include "MayaFlux/Nodes/Network/Operators/ParticleFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/PhysicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void ParticleGeometryBuffer::setup_processors(ProcessingToken token)
{
    NetworkGeometryBuffer::setup_processors(token);

    auto network = get_network();
    auto op_chain = network ? network->get_operator_chain() : nullptr;
    if (!op_chain) {
        return;
    }

    for (const auto& op : op_chain->operators()) {
        auto particle_op = std::dynamic_pointer_cast<Nodes::Network::ParticleFieldOperator>(op);
        if (particle_op) {
            wire_particle_field_operator(particle_op);
            return;
        }
    }
}

void ParticleGeometryBuffer::wire_particle_field_operator(
    const std::shared_ptr<Nodes::Network::ParticleFieldOperator>& particle_op)
{
    auto self = std::dynamic_pointer_cast<NetworkGeometryBuffer>(shared_from_this());
    auto chain = get_processing_chain();
    if (!self || !chain) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "ParticleGeometryBuffer: no processing chain to wire particle stages onto");
        return;
    }

    if (particle_op->binding_count() > 0) {
        chain->add_postprocessor(std::make_shared<VertexFieldProcessor>(particle_op), self);
    }

    const auto& pconfig = particle_op->get_particle_config();
    if (!pconfig.absorb_radius.has_value() && !pconfig.density_color) {
        return;
    }

    auto* physics = dynamic_cast<Nodes::Network::PhysicsOperator*>(get_network()->get_operator());

    float cell_size = 0.0F;
    if (pconfig.cell_size.has_value()) {
        cell_size = *pconfig.cell_size;
    } else if (physics) {
        cell_size = physics->get_interaction_radius();
    } else {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "ParticleGeometryBuffer: ParticleFieldConfig has no cell_size and the network's "
            "primary operator is not a PhysicsOperator, so one cannot be derived");
        return;
    }

    auto hash_config = SpatialHashConfig::from_network(self, cell_size);
    if (!hash_config.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "ParticleGeometryBuffer: SpatialHashConfig::from_network failed");
        return;
    }

    const uint32_t live_count = hash_config->particle_count;
    bool reserve_enabled = pconfig.reserve_fraction > 0.0F;

    if (reserve_enabled && !pconfig.absorb_radius.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "ParticleGeometryBuffer: reserve_fraction requires absorb_radius (destroy-on-"
            "absorption has nothing to destroy without claims); reserve capacity disabled");
        reserve_enabled = false;
    }

    uint32_t total_count = live_count;
    if (reserve_enabled) {
        const auto reserve_count = static_cast<uint32_t>(
            std::ceil(static_cast<float>(live_count) * pconfig.reserve_fraction));
        total_count = live_count + reserve_count;

        const size_t needed_bytes = static_cast<size_t>(total_count) * hash_config->stride_words * 4;
        self->resize(needed_bytes, true);

        hash_config->particle_count = total_count;
    }

    hash_config->declare_fields(self);

    if (total_count != live_count) {
        std::vector<uint32_t> cluster_ids(total_count, 0U);
        if (physics) {
            auto live_ids = physics->build_cluster_ids();
            std::copy_n(live_ids.begin(),
                std::min<size_t>(live_ids.size(), live_count), cluster_ids.begin());
        }
        std::shared_ptr<VKBuffer> cluster_staging;
        upload_back_buffer(self->write_state_slot("hash_cluster_id"), cluster_ids.data(),
            cluster_ids.size() * sizeof(uint32_t), cluster_staging);
    } else {
        self->ensure_cluster_ids();
    }

    std::optional<PopulationConfig> pop_config;
    if (reserve_enabled) {
        pop_config = PopulationConfig { .hash = *hash_config, .live_count = live_count };
        pop_config->declare_fields(self);
        chain->add_processor(std::make_shared<PopulationInitProcessor>(*pop_config), self);
    }

    chain->add_processor(std::make_shared<HashClearProcessor>(*hash_config), self);
    chain->add_processor(std::make_shared<HashCountProcessor>(*hash_config, reserve_enabled), self);
    chain->add_processor(std::make_shared<HashScanProcessor>(*hash_config), self);
    chain->add_processor(std::make_shared<HashScatterProcessor>(*hash_config, reserve_enabled), self);

    if (pconfig.density_color) {
        chain->add_processor(
            std::make_shared<HashDensityColorProcessor>(*hash_config, particle_op), self);
    }

    if (!pconfig.absorb_radius.has_value()) {
        return;
    }

    MutationConfig claim_config { .hash = *hash_config, .absorb_radius = *pconfig.absorb_radius };
    claim_config.declare_fields(self);

    chain->add_processor(std::make_shared<ClaimInitProcessor>(claim_config), self);
    chain->add_processor(
        std::make_shared<ClaimProcessor>(claim_config, particle_op, reserve_enabled), self);
    chain->add_processor(std::make_shared<ClaimFlattenProcessor>(claim_config), self);
    chain->add_processor(
        std::make_shared<ClaimAccumulateProcessor>(
            claim_config, physics, reserve_enabled ? live_count : 0U, particle_op),
        self);

    if (pconfig.cosmetic_swallow) {
        chain->add_processor(std::make_shared<ClaimSwallowProcessor>(claim_config, particle_op), self);
    }

    if (pop_config.has_value()) {
        chain->add_processor(
            std::make_shared<PopulationSpawnProcessor>(*pop_config, particle_op), self);
    }
}

} // namespace MayaFlux::Buffers
