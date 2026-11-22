#include "SpatialFieldWiring.hpp"

#include "MutationClaimProcessor.hpp"
#include "PopulationProcessor.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/VertexFieldProcessor.hpp"
#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {

    /**
     * @brief Adds the field pass to the chain on scope exit, after every spatial
     *        stage the enclosing scope went on to add.
     *
     * The field pass must be the last stage to write vertex records, which is
     * what the postprocess slot used to buy it. Expressing the same order in the
     * flat process list means adding it after the hash, claim and population
     * stages, and wire_field_stages leaves by several paths, most of which add
     * no stages at all. Deferring to scope exit keeps one ordering rule true on
     * all of them without splitting the wiring up.
     *
     * A null chain means no pass is wanted, which is the operator carrying no
     * bindings to dispatch.
     */
    struct DeferredFieldPass {
        BufferProcessingChain* chain;
        std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op;
        std::shared_ptr<StateFieldBuffer> buffer;

        ~DeferredFieldPass()
        {
            if (!chain) {
                return;
            }
            try {
                chain->add_processor(std::make_shared<VertexFieldProcessor>(field_op), buffer);
            } catch (const std::exception& e) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                    "wire_field_stages: field pass could not be wired: {}", e.what());
            }
        }
    };

} // namespace

bool wants_spatial_stages(const Nodes::Network::SpatialFieldConfig& config)
{
    return config.absorb_radius.has_value() || config.density_color;
}

void wire_field_stages(
    const std::shared_ptr<StateFieldBuffer>& buffer,
    const std::shared_ptr<Nodes::Network::GpuFieldOperator>& field_op,
    const std::optional<SpatialHashConfig>& hash,
    const std::vector<uint32_t>& cluster_ids,
    Nodes::Network::PhysicsOperator* physics)
{
    auto chain = buffer ? buffer->get_processing_chain() : nullptr;
    if (!chain || !field_op) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "wire_field_stages: no processing chain or field operator to wire field stages onto");
        return;
    }

    const DeferredFieldPass field_pass {
        .chain = field_op->binding_count() > 0 ? chain.get() : nullptr,
        .field_op = field_op,
        .buffer = buffer
    };

    const auto& pconfig = field_op->get_field_config();
    if (!wants_spatial_stages(pconfig)) {
        return;
    }

    if (pconfig.transfer_on_claim && !pconfig.absorb_radius.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "wire_field_stages: transfer_on_claim requires absorb_radius (there is no "
            "claim to reinterpret without it); transfer disabled");
    }

    if (!hash.has_value()) {
        return;
    }

    SpatialHashConfig hash_config = *hash;
    const uint32_t live_count = hash_config.particle_count;
    bool reserve_enabled = pconfig.reserve_fraction > 0.0F;

    if (reserve_enabled && !pconfig.absorb_radius.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "wire_field_stages: reserve_fraction requires absorb_radius (destroy-on-"
            "absorption has nothing to destroy without claims); reserve capacity disabled");
        reserve_enabled = false;
    }

    uint32_t total_count = live_count;
    if (reserve_enabled) {
        const auto reserve_count = static_cast<uint32_t>(
            std::ceil(static_cast<float>(live_count) * pconfig.reserve_fraction));
        total_count = live_count + reserve_count;

        const size_t needed_bytes = static_cast<size_t>(total_count) * hash_config.stride_words * 4;
        buffer->resize(needed_bytes, true);

        hash_config.particle_count = total_count;
    }

    hash_config.declare_fields(buffer);

    if (total_count > 0) {
        std::vector<uint32_t> padded_ids(total_count, 0U);
        std::copy_n(cluster_ids.begin(),
            std::min<size_t>(cluster_ids.size(), live_count), padded_ids.begin());
        std::shared_ptr<VKBuffer> cluster_staging;
        upload_back_buffer(buffer->write_state_slot("hash_cluster_id"), padded_ids.data(),
            padded_ids.size() * sizeof(uint32_t), cluster_staging);
    }

    std::optional<PopulationConfig> pop_config;
    if (reserve_enabled) {
        pop_config = PopulationConfig { .hash = hash_config, .live_count = live_count };
        pop_config->declare_fields(buffer);
        chain->add_processor(std::make_shared<PopulationInitProcessor>(*pop_config), buffer);
    }

    chain->add_processor(std::make_shared<HashClearProcessor>(hash_config), buffer);
    chain->add_processor(std::make_shared<HashCountProcessor>(hash_config, reserve_enabled), buffer);
    chain->add_processor(std::make_shared<HashScanProcessor>(hash_config), buffer);
    chain->add_processor(std::make_shared<HashScatterProcessor>(hash_config, reserve_enabled), buffer);

    if (pconfig.density_color) {
        chain->add_processor(
            std::make_shared<HashDensityColorProcessor>(hash_config, field_op), buffer);
    }

    if (!pconfig.absorb_radius.has_value()) {
        return;
    }

    MutationConfig claim_config { .hash = hash_config, .absorb_radius = *pconfig.absorb_radius };
    claim_config.declare_fields(buffer);

    const bool transfer_on_claim = pconfig.transfer_on_claim;

    chain->add_processor(std::make_shared<ClaimInitProcessor>(claim_config), buffer);
    chain->add_processor(
        std::make_shared<ClaimProcessor>(claim_config, field_op, reserve_enabled), buffer);
    chain->add_processor(std::make_shared<ClaimFlattenProcessor>(claim_config), buffer);
    chain->add_processor(
        std::make_shared<ClaimAccumulateProcessor>(
            claim_config, physics, reserve_enabled ? live_count : 0U, field_op, transfer_on_claim),
        buffer);

    if (pconfig.cosmetic_swallow) {
        chain->add_processor(
            std::make_shared<ClaimSwallowProcessor>(claim_config, field_op, transfer_on_claim), buffer);
    }

    if (transfer_on_claim) {
        chain->add_processor(std::make_shared<ClaimTransferProcessor>(claim_config), buffer);
    }

    if (pop_config.has_value()) {
        chain->add_processor(
            std::make_shared<PopulationSpawnProcessor>(*pop_config, field_op), buffer);
    }
}

} // namespace MayaFlux::Buffers
