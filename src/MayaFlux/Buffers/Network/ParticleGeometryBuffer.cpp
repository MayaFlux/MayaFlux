#include "ParticleGeometryBuffer.hpp"

#include "MutationClaimProcessor.hpp"
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

namespace {

    /**
     * @brief Per-particle cluster id, global index order, derived from a
     *        PhysicsOperator's own collections.
     * @return All zero, sized to particle_count, when physics is null or
     *         carries at most one collection. Otherwise entry i is the
     *         index of the CollectionGroup particle i belongs to, in
     *         PhysicsOperator::get_collections() order.
     *
     * Cluster membership is fixed once a collection is added; nothing here
     * runs per cycle. The caller uploads the result exactly once, at wiring
     * time, into SpatialHashConfig's own hash_cluster_id field -- shared by
     * HashDensityColorProcessor and the claim protocol alike, so it is built
     * whenever hashing happens at all, not only when absorb_radius is set.
     */
    std::vector<uint32_t> build_cluster_ids(
        Nodes::Network::PhysicsOperator* physics,
        size_t particle_count)
    {
        std::vector<uint32_t> ids(particle_count, 0U);

        if (!physics || physics->get_collections().size() <= 1) {
            return ids;
        }

        size_t offset = 0;
        uint32_t cluster = 0;
        for (const auto& group : physics->get_collections()) {
            const size_t count = group.collection->get_point_count();
            for (size_t i = 0; i < count && offset + i < particle_count; ++i) {
                ids[offset + i] = cluster;
            }
            offset += count;
            ++cluster;
        }

        return ids;
    }

} // namespace

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

    hash_config->declare_fields(self);

    auto cluster_ids = build_cluster_ids(physics, hash_config->particle_count);
    std::shared_ptr<VKBuffer> cluster_staging;
    upload_back_buffer(
        self->write_state_slot("hash_cluster_id"),
        cluster_ids.data(),
        cluster_ids.size() * sizeof(uint32_t),
        cluster_staging);

    chain->add_processor(std::make_shared<HashClearProcessor>(*hash_config), self);
    chain->add_processor(std::make_shared<HashCountProcessor>(*hash_config), self);
    chain->add_processor(std::make_shared<HashScanProcessor>(*hash_config), self);
    chain->add_processor(std::make_shared<HashScatterProcessor>(*hash_config), self);

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
    chain->add_processor(std::make_shared<ClaimProcessor>(claim_config, particle_op), self);
    chain->add_processor(std::make_shared<ClaimFlattenProcessor>(claim_config), self);
    chain->add_processor(std::make_shared<ClaimAccumulateProcessor>(claim_config, physics), self);

    if (pconfig.cosmetic_swallow) {
        chain->add_processor(std::make_shared<ClaimSwallowProcessor>(claim_config, particle_op), self);
    }
}

} // namespace MayaFlux::Buffers
