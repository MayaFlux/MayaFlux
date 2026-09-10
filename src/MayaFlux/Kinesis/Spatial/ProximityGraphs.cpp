#include "ProximityGraphs.hpp"

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include <queue>

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis {

namespace {

    /**
     * @brief Largest point count for which the pairwise table is materialised.
     *
     * The table costs n^2 doubles and is only built for the cubic-time graphs,
     * where it amortises to nothing. Above this the witness scans read the
     * point coordinates directly.
     */
    constexpr size_t k_max_table_points = 8192;

    /**
     * @brief Squared distance between two column-major points.
     * @param a First point, @p dim contiguous doubles.
     * @param b Second point, @p dim contiguous doubles.
     * @param dim Coordinate count.
     */
    [[nodiscard]] inline double dist_sq(const double* a, const double* b, size_t dim) noexcept
    {
        double sum = 0.0;
        for (size_t c = 0; c < dim; ++c) {
            const double delta = b[c] - a[c];
            sum += delta * delta;
        }
        return sum;
    }

    /**
     * @brief Whether any k satisfies row_i[k] + row_j[k] < threshold.
     *
     * The Gabriel rejection test. k equal to i or j can never satisfy it:
     * both reduce to threshold < threshold, so the scan needs no exclusion.
     */
    [[nodiscard]] inline bool sum_below(
        const double* row_i, const double* row_j, size_t n, double threshold) noexcept
    {
        size_t k = 0;

#ifdef MAYAFLUX_ARCH_X64
        const __m256d limit = _mm256_set1_pd(threshold);
        for (; k + 4 <= n; k += 4) {
            const __m256d sum = _mm256_add_pd(
                _mm256_loadu_pd(row_i + k),
                _mm256_loadu_pd(row_j + k));
            if (_mm256_movemask_pd(_mm256_cmp_pd(sum, limit, _CMP_LT_OS)) != 0) {
                return true;
            }
        }
#elif defined(MAYAFLUX_ARCH_ARM64)
        const float64x2_t limit = vdupq_n_f64(threshold);
        for (; k + 2 <= n; k += 2) {
            const float64x2_t sum = vaddq_f64(vld1q_f64(row_i + k), vld1q_f64(row_j + k));
            const uint64x2_t mask = vcltq_f64(sum, limit);
            if ((vgetq_lane_u64(mask, 0) | vgetq_lane_u64(mask, 1)) != 0) {
                return true;
            }
        }
#endif

        for (; k < n; ++k) {
            if (row_i[k] + row_j[k] < threshold) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Whether any k satisfies max(row_i[k], row_j[k]) < threshold.
     *
     * The relative neighborhood rejection test. As with sum_below, k equal to
     * i or j reduces to threshold < threshold and cannot trigger.
     */
    [[nodiscard]] inline bool max_below(
        const double* row_i, const double* row_j, size_t n, double threshold) noexcept
    {
        size_t k = 0;

#ifdef MAYAFLUX_ARCH_X64
        const __m256d limit = _mm256_set1_pd(threshold);
        for (; k + 4 <= n; k += 4) {
            const __m256d hi = _mm256_max_pd(
                _mm256_loadu_pd(row_i + k),
                _mm256_loadu_pd(row_j + k));
            if (_mm256_movemask_pd(_mm256_cmp_pd(hi, limit, _CMP_LT_OS)) != 0) {
                return true;
            }
        }
#elif defined(MAYAFLUX_ARCH_ARM64)
        const float64x2_t limit = vdupq_n_f64(threshold);
        for (; k + 2 <= n; k += 2) {
            const float64x2_t hi = vmaxq_f64(vld1q_f64(row_i + k), vld1q_f64(row_j + k));
            const uint64x2_t mask = vcltq_f64(hi, limit);
            if ((vgetq_lane_u64(mask, 0) | vgetq_lane_u64(mask, 1)) != 0) {
                return true;
            }
        }
#endif

        for (; k < n; ++k) {
            if (std::max(row_i[k], row_j[k]) < threshold) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Gabriel rejection test reading coordinates, for point counts
     *        above k_max_table_points.
     */
    [[nodiscard]] bool sum_below_direct(
        const double* base, size_t dim, size_t n,
        size_t i, size_t j, double threshold) noexcept
    {
        const double* pi = base + i * dim;
        const double* pj = base + j * dim;

        for (size_t k = 0; k < n; ++k) {
            if (k == i || k == j) {
                continue;
            }
            const double* pk = base + k * dim;
            if (dist_sq(pi, pk, dim) + dist_sq(pj, pk, dim) < threshold) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Relative neighborhood rejection test reading coordinates, for
     *        point counts above k_max_table_points.
     */
    [[nodiscard]] bool max_below_direct(
        const double* base, size_t dim, size_t n,
        size_t i, size_t j, double threshold) noexcept
    {
        const double* pi = base + i * dim;
        const double* pj = base + j * dim;

        for (size_t k = 0; k < n; ++k) {
            if (k == i || k == j) {
                continue;
            }
            const double* pk = base + k * dim;
            if (std::max(dist_sq(pi, pk, dim), dist_sq(pj, pk, dim)) < threshold) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Materialise the full pairwise squared distance table, row major.
     * @return n*n doubles, or empty when n exceeds k_max_table_points.
     *
     * Only worth building for the cubic-time graphs. The quadratic ones pay
     * the same order to build it as to run, so they read coordinates instead.
     */
    [[nodiscard]] std::vector<double> build_distance_table(const Eigen::MatrixXd& points)
    {
        const auto n = static_cast<size_t>(points.cols());
        if (n > k_max_table_points) {
            return {};
        }

        const auto dim = static_cast<size_t>(points.rows());
        const double* base = points.data();

        std::vector<double> table(n * n);

        P::for_each(P::par_unseq,
            std::views::iota(size_t { 0 }, n).begin(),
            std::views::iota(size_t { 0 }, n).end(),
            [&](size_t i) {
                const double* pi = base + i * dim;
                double* row = table.data() + i * n;
                for (size_t j = 0; j < n; ++j) {
                    row[j] = dist_sq(pi, base + j * dim, dim);
                }
            });

        return table;
    }

    /**
     * @brief Concatenate per-source edge bins in ascending source order.
     *
     * Preserves the emission order of the serial nested loop, so the result
     * is independent of how the parallel scheduler split the work.
     */
    [[nodiscard]] EdgeList flatten_bins(std::vector<EdgeList>& bins)
    {
        size_t total = 0;
        for (const EdgeList& bin : bins) {
            total += bin.size();
        }

        EdgeList edges;
        edges.reserve(total);

        for (EdgeList& bin : bins) {
            edges.insert(edges.end(), bin.begin(), bin.end());
        }

        return edges;
    }

    struct Edge {
        size_t a, b;
        double weight;
        bool operator>(const Edge& other) const { return weight > other.weight; }
    };

} // namespace

EdgeList sequential_chain(const Eigen::MatrixXd& points)
{
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    EdgeList edges;
    edges.reserve(n - 1);

    for (Eigen::Index i = 0; i < n - 1; ++i) {
        edges.emplace_back(static_cast<size_t>(i), static_cast<size_t>(i + 1));
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "sequential_chain: {} points, generated {} edges", n, edges.size());

    return edges;
}

EdgeList k_nearest_neighbors(
    const Eigen::MatrixXd& points,
    size_t k)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    k = std::min(k, n - 1);
    if (k == 0) {
        return {};
    }

    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    EdgeList edges(n * k);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            std::vector<std::pair<double, size_t>> distances;
            distances.reserve(n - 1);

            const double* pi = base + i * dim;
            for (size_t j = 0; j < n; ++j) {
                if (i == j) {
                    continue;
                }
                distances.emplace_back(dist_sq(pi, base + j * dim, dim), j);
            }

            std::partial_sort(
                distances.begin(),
                distances.begin() + static_cast<ptrdiff_t>(k),
                distances.end());

            for (size_t m = 0; m < k; ++m) {
                edges[i * k + m] = { i, distances[m].second };
            }
        });

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "k_nearest_neighbors: {} points, k={}, generated {} edges",
        n, k, edges.size());

    return edges;
}

EdgeList radius_threshold_graph(
    const Eigen::MatrixXd& points,
    double radius)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    const double radius_sq = radius * radius;
    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    std::vector<size_t> offsets(n + 1, 0);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const double* pi = base + i * dim;
            size_t count = 0;
            for (size_t j = i + 1; j < n; ++j) {
                if (dist_sq(pi, base + j * dim, dim) <= radius_sq) {
                    ++count;
                }
            }
            offsets[i + 1] = count;
        });

    for (size_t i = 0; i < n; ++i) {
        offsets[i + 1] += offsets[i];
    }

    EdgeList edges(offsets[n]);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const double* pi = base + i * dim;
            size_t at = offsets[i];
            for (size_t j = i + 1; j < n; ++j) {
                if (dist_sq(pi, base + j * dim, dim) <= radius_sq) {
                    edges[at++] = { i, j };
                }
            }
        });

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "radius_threshold_graph: {} points, radius={:.3f}, generated {} edges",
        n, radius, edges.size());

    return edges;
}

EdgeList minimum_spanning_tree(const Eigen::MatrixXd& points)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    EdgeList mst_edges;
    mst_edges.reserve(n - 1);

    std::vector<bool> in_mst(n, false);
    std::priority_queue<Edge, std::vector<Edge>, std::greater<>> pq;

    in_mst[0] = true;

    for (size_t j = 1; j < n; ++j) {
        pq.push({ .a = 0, .b = j, .weight = std::sqrt(dist_sq(base, base + j * dim, dim)) });
    }

    while (!pq.empty() && mst_edges.size() < n - 1) {
        const Edge e = pq.top();
        pq.pop();

        if (in_mst[e.b]) {
            continue;
        }

        mst_edges.emplace_back(e.a, e.b);
        in_mst[e.b] = true;

        const double* pb = base + e.b * dim;
        for (size_t j = 0; j < n; ++j) {
            if (!in_mst[j]) {
                pq.push({ .a = e.b, .b = j, .weight = std::sqrt(dist_sq(pb, base + j * dim, dim)) });
            }
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "minimum_spanning_tree: {} points, generated {} edges",
        n, mst_edges.size());

    return mst_edges;
}

EdgeList gabriel_graph(const Eigen::MatrixXd& points)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    const std::vector<double> table = build_distance_table(points);
    const bool resident = !table.empty();

    std::vector<EdgeList> bins(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const double* row_i = resident ? table.data() + i * n : nullptr;
            const double* pi = base + i * dim;
            EdgeList& out = bins[i];

            for (size_t j = i + 1; j < n; ++j) {
                const double pq_sq = resident
                    ? row_i[j]
                    : dist_sq(pi, base + j * dim, dim);

                const bool rejected = resident
                    ? sum_below(row_i, table.data() + j * n, n, pq_sq)
                    : sum_below_direct(base, dim, n, i, j, pq_sq);

                if (!rejected) {
                    out.emplace_back(i, j);
                }
            }
        });

    EdgeList edges = flatten_bins(bins);

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "gabriel_graph: {} points, generated {} edges",
        n, edges.size());

    return edges;
}

EdgeList nearest_neighbor_graph(const Eigen::MatrixXd& points)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    std::vector<size_t> nearest(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            double min_dist_sq = std::numeric_limits<double>::max();
            size_t best = i;

            const double* pi = base + i * dim;
            for (size_t j = 0; j < n; ++j) {
                if (i == j) {
                    continue;
                }
                const double d = dist_sq(pi, base + j * dim, dim);
                if (d < min_dist_sq) {
                    min_dist_sq = d;
                    best = j;
                }
            }

            nearest[i] = best;
        });

    EdgeList edges;
    edges.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        if (nearest[i] != i) {
            edges.emplace_back(i, nearest[i]);
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "nearest_neighbor_graph: {} points, generated {} edges", n, edges.size());

    return edges;
}

EdgeList relative_neighborhood_graph(const Eigen::MatrixXd& points)
{
    const auto n = static_cast<size_t>(points.cols());
    if (n < 2) {
        return {};
    }

    const auto dim = static_cast<size_t>(points.rows());
    const double* base = points.data();

    const std::vector<double> table = build_distance_table(points);
    const bool resident = !table.empty();

    std::vector<EdgeList> bins(n);

    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, n).begin(),
        std::views::iota(size_t { 0 }, n).end(),
        [&](size_t i) {
            const double* row_i = resident ? table.data() + i * n : nullptr;
            const double* pi = base + i * dim;
            EdgeList& out = bins[i];

            for (size_t j = i + 1; j < n; ++j) {
                const double pq_sq = resident
                    ? row_i[j]
                    : dist_sq(pi, base + j * dim, dim);

                const bool rejected = resident
                    ? max_below(row_i, table.data() + j * n, n, pq_sq)
                    : max_below_direct(base, dim, n, i, j, pq_sq);

                if (!rejected) {
                    out.emplace_back(i, j);
                }
            }
        });

    EdgeList edges = flatten_bins(bins);

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "relative_neighborhood_graph: {} points, generated {} edges",
        n, edges.size());

    return edges;
}

EdgeList custom_proximity_graph(
    const Eigen::MatrixXd& points,
    const std::function<EdgeList(const Eigen::MatrixXd&)>& connection_function)
{
    if (!connection_function) {
        MF_ERROR(Journal::Component::Kinesis, Journal::Context::Runtime,
            "custom_proximity_graph: connection_function is null");
        return {};
    }

    EdgeList edges = connection_function(points);

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "custom_proximity_graph: {} points, generated {} edges",
        points.cols(), edges.size());

    return edges;
}

EdgeList generate_proximity_graph(
    const Eigen::MatrixXd& points,
    const ProximityConfig& config)
{
    switch (config.mode) {

    case ProximityMode::SEQUENTIAL:
        return sequential_chain(points);

    case ProximityMode::K_NEAREST:
        return k_nearest_neighbors(points, config.k_neighbors);

    case ProximityMode::RADIUS_THRESHOLD:
        return radius_threshold_graph(points, config.radius);

    case ProximityMode::MINIMUM_SPANNING_TREE:
        return minimum_spanning_tree(points);

    case ProximityMode::GABRIEL_GRAPH:
        return gabriel_graph(points);

    case ProximityMode::NEAREST_NEIGHBOR:
        return nearest_neighbor_graph(points);

    case ProximityMode::RELATIVE_NEIGHBORHOOD_GRAPH:
        return relative_neighborhood_graph(points);

    case ProximityMode::CUSTOM:
        return custom_proximity_graph(points, config.custom_function);

    default:
        return {};
    }
}

} // namespace MayaFlux::Kinesis
