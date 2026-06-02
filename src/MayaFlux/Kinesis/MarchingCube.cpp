#include "GeometryPrimitives.hpp"

#include "KMarchingTable.hpp"
#include "MayaFlux/Kakshya/NDData/MeshInsertion.hpp"

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

namespace MayaFlux::Kinesis {

[[nodiscard]] inline uint32_t compute_cube_index(const float* v, float iso) noexcept
{
#ifdef MAYAFLUX_ARCH_X64
    const __m256 vals = _mm256_loadu_ps(v);
    const __m256 threshold = _mm256_set1_ps(iso);
    const __m256 cmp = _mm256_cmp_ps(vals, threshold, _CMP_LT_OS);
    return static_cast<uint32_t>(_mm256_movemask_ps(cmp));
#elif defined(MAYAFLUX_ARCH_ARM64)
    const float32x4_t lo = vld1q_f32(v);
    const float32x4_t hi = vld1q_f32(v + 4);
    const float32x4_t thresh = vdupq_n_f32(iso);

    // vcltq_f32 -> 0xFFFFFFFF per lane where v[i] < iso, 0x00000000 otherwise.
    // Shift each lane right by (31 - bit_position) to land the set bit at the
    // correct position in the final scalar, then sum across lanes.
    const uint32x4_t lo_mask = vreinterpretq_u32_u32(vcltq_f32(lo, thresh));
    const uint32x4_t hi_mask = vreinterpretq_u32_u32(vcltq_f32(hi, thresh));

    // Isolate bit 0 of each lane (the sign bit after the comparison saturates).
    // vshrq_n_u32 by 31 collapses each 0xFFFFFFFF to 1 and 0x00000000 to 0.
    alignas(16) static const uint32_t k_lo_shifts[4] = { 31, 30, 29, 28 };
    alignas(16) static const uint32_t k_hi_shifts[4] = { 27, 26, 25, 24 };

    const uint32x4_t lo_bits = vshlq_u32(vshrq_n_u32(lo_mask, 31),
        vld1q_s32(reinterpret_cast<const int32_t*>(k_lo_shifts)));
    const uint32x4_t hi_bits = vshlq_u32(vshrq_n_u32(hi_mask, 31),
        vld1q_s32(reinterpret_cast<const int32_t*>(k_hi_shifts)));

    const uint32x4_t combined = vorrq_u32(lo_bits, hi_bits);
    return vaddvq_u32(combined);
#else
    uint32_t idx = 0;
    idx |= static_cast<uint32_t>(v[0] < iso) << 0;
    idx |= static_cast<uint32_t>(v[1] < iso) << 1;
    idx |= static_cast<uint32_t>(v[2] < iso) << 2;
    idx |= static_cast<uint32_t>(v[3] < iso) << 3;
    idx |= static_cast<uint32_t>(v[4] < iso) << 4;
    idx |= static_cast<uint32_t>(v[5] < iso) << 5;
    idx |= static_cast<uint32_t>(v[6] < iso) << 6;
    idx |= static_cast<uint32_t>(v[7] < iso) << 7;
    return idx;
#endif
}

// =============================================================================
// generate_sdf_mesh
// =============================================================================

struct InterpolatedEdge {
    glm::vec3 position;
    glm::vec3 normal;
};

Kakshya::MeshData generate_sdf_mesh(
    const Kinesis::SpatialField& field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float iso_level)
{
    res_x = std::max(res_x, 1U);
    res_y = std::max(res_y, 1U);
    res_z = std::max(res_z, 1U);

    const glm::vec3 extent = bounds_max - bounds_min;
    const glm::vec3 step {
        extent.x / static_cast<float>(res_x),
        extent.y / static_cast<float>(res_y),
        extent.z / static_cast<float>(res_z),
    };

    const uint32_t nx = res_x + 1;
    const uint32_t ny = res_y + 1;
    const uint32_t nz = res_z + 1;

    const uint32_t px = nx + 2;
    const uint32_t py = ny + 2;
    const uint32_t pz = nz + 2;
    const size_t num_voxels = static_cast<size_t>(px) * py * pz;

    const size_t sx = 1;
    const size_t sy = px;
    const size_t sz = static_cast<size_t>(py) * px;

    std::vector<float> vals(num_voxels);
    for (uint32_t iz = 0; iz < pz; ++iz) {
        float z = bounds_min.z + static_cast<float>(static_cast<int32_t>(iz) - 1) * step.z;
        for (uint32_t iy = 0; iy < py; ++iy) {
            float y = bounds_min.y + static_cast<float>(static_cast<int32_t>(iy) - 1) * step.y;
            size_t base_idx = (static_cast<size_t>(iz) * py + iy) * px;
            for (uint32_t ix = 0; ix < px; ++ix) {
                float x = bounds_min.x + static_cast<float>(static_cast<int32_t>(ix) - 1) * step.x;
                vals[base_idx + ix] = field(glm::vec3(x, y, z));
            }
        }
    }

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(static_cast<size_t>(res_x * res_y * res_z) * 3);
    indices.reserve(static_cast<size_t>(res_x * res_y * res_z) * 3);

    auto grad_at = [&](size_t ptr) -> glm::vec3 {
        glm::vec3 n {
            (vals[ptr + sx] - vals[ptr - sx]) / step.x,
            (vals[ptr + sy] - vals[ptr - sy]) / step.y,
            (vals[ptr + sz] - vals[ptr - sz]) / step.z
        };
        float len = glm::length(n);
        return len > 1e-7F ? (n / len) : glm::vec3(0.0F, 1.0F, 0.0F);
    };

    for (uint32_t iz = 0; iz < res_z; ++iz) {
        float z0 = bounds_min.z + static_cast<float>(iz) * step.z;
        float z1 = z0 + step.z;

        for (uint32_t iy = 0; iy < res_y; ++iy) {
            float y0 = bounds_min.y + static_cast<float>(iy) * step.y;
            float y1 = y0 + step.y;

            size_t voxel_ptr = (static_cast<size_t>(iz + 1) * py + (iy + 1)) * px + 1;

            for (uint32_t ix = 0; ix < res_x; ++ix, ++voxel_ptr) {
                const float v[8] = {
                    vals[voxel_ptr], vals[voxel_ptr + sx], vals[voxel_ptr + sy + sx], vals[voxel_ptr + sy],
                    vals[voxel_ptr + sz], vals[voxel_ptr + sz + sx], vals[voxel_ptr + sz + sy + sx], vals[voxel_ptr + sz + sy]
                };

                const uint32_t cube_idx = compute_cube_index(v, iso_level);
                const uint16_t edges = k_edge_table[cube_idx];
                if (edges == 0)
                    continue;

                float x0 = bounds_min.x + static_cast<float>(ix) * step.x;
                float x1 = x0 + step.x;
                const glm::vec3 c[8] = {
                    { x0, y0, z0 }, { x1, y0, z0 }, { x1, y1, z0 }, { x0, y1, z0 },
                    { x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }
                };

                const glm::vec3 nrm[8] = {
                    grad_at(voxel_ptr), grad_at(voxel_ptr + sx), grad_at(voxel_ptr + sy + sx), grad_at(voxel_ptr + sy),
                    grad_at(voxel_ptr + sz), grad_at(voxel_ptr + sz + sx), grad_at(voxel_ptr + sz + sy + sx), grad_at(voxel_ptr + sz + sy)
                };

                InterpolatedEdge ep[12];

                auto interp_edge = [&](int i1, int i2) -> InterpolatedEdge {
                    float va = v[i1], vb = v[i2];
                    float d = vb - va;
                    float t = (std::abs(d) < 1e-7F) ? 0.5F : ((iso_level - va) / d);

                    return {
                        .position = c[i1] + t * (c[i2] - c[i1]),
                        .normal = glm::normalize(nrm[i1] + t * (nrm[i2] - nrm[i1]))
                    };
                };

                if (edges & 0x001)
                    ep[0] = interp_edge(0, 1);
                if (edges & 0x002)
                    ep[1] = interp_edge(1, 2);
                if (edges & 0x004)
                    ep[2] = interp_edge(2, 3);
                if (edges & 0x008)
                    ep[3] = interp_edge(3, 0);
                if (edges & 0x010)
                    ep[4] = interp_edge(4, 5);
                if (edges & 0x020)
                    ep[5] = interp_edge(5, 6);
                if (edges & 0x040)
                    ep[6] = interp_edge(6, 7);
                if (edges & 0x080)
                    ep[7] = interp_edge(7, 4);
                if (edges & 0x100)
                    ep[8] = interp_edge(0, 4);
                if (edges & 0x200)
                    ep[9] = interp_edge(1, 5);
                if (edges & 0x400)
                    ep[10] = interp_edge(2, 6);
                if (edges & 0x800)
                    ep[11] = interp_edge(3, 7);

                const auto& tri = k_tri_table[cube_idx];

                for (int i = 0; tri[i] != -1; i += 3) {
                    const auto base = static_cast<uint32_t>(verts.size());

                    for (int j = 0; j < 3; ++j) {
                        const auto& edge_data = ep[tri[i + j]];
                        glm::vec2 uv((edge_data.position.x - bounds_min.x) / extent.x,
                            (edge_data.position.y - bounds_min.y) / extent.y);

                        verts.push_back({ .position = edge_data.position,
                            .uv = uv,
                            .normal = edge_data.normal });
                    }
                    indices.insert(indices.end(), { base, base + 1, base + 2 });
                }
            }
        }
    }

    if (verts.empty())
        return Kakshya::MeshData::empty();

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()), verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));

    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    data.layout.vertex_count = static_cast<uint32_t>(verts.size());
    return data;
}

} // namespace MayaFlux::Kinesis
