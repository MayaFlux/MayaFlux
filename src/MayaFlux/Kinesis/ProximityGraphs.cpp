#include "ProximityGraphs.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include <queue>

namespace MayaFlux::Kinesis {

namespace {
    double distance_squared(const Eigen::VectorXd& a, const Eigen::VectorXd& b)
    {
        return (b - a).squaredNorm();
    }

    double distance(const Eigen::VectorXd& a, const Eigen::VectorXd& b)
    {
        return (b - a).norm();
    }

    struct Edge {
        size_t a, b;
        double weight;
        bool operator>(const Edge& other) const { return weight > other.weight; }
    };
}

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
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    k = std::min(k, static_cast<size_t>(n - 1));

    EdgeList edges;
    edges.reserve(n * k);

    for (Eigen::Index i = 0; i < n; ++i) {
        std::vector<std::pair<double, size_t>> distances;
        distances.reserve(n - 1);

        for (Eigen::Index j = 0; j < n; ++j) {
            if (i == j)
                continue;
            double dist_sq = distance_squared(points.col(i), points.col(j));
            distances.emplace_back(dist_sq, static_cast<size_t>(j));
        }

        std::partial_sort(
            distances.begin(),
            distances.begin() + k,
            distances.end());

        for (size_t neighbor_idx = 0; neighbor_idx < k; ++neighbor_idx) {
            edges.emplace_back(static_cast<size_t>(i), distances[neighbor_idx].second);
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "k_nearest_neighbors: {} points, k={}, generated {} edges",
        n, k, edges.size());

    return edges;
}

EdgeList radius_threshold_graph(
    const Eigen::MatrixXd& points,
    double radius)
{
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    double radius_sq = radius * radius;
    EdgeList edges;

    for (Eigen::Index i = 0; i < n; ++i) {
        for (Eigen::Index j = i + 1; j < n; ++j) {
            double dist_sq = distance_squared(points.col(i), points.col(j));
            if (dist_sq <= radius_sq) {
                edges.emplace_back(static_cast<size_t>(i), static_cast<size_t>(j));
            }
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "radius_threshold_graph: {} points, radius={:.3f}, generated {} edges",
        n, radius, edges.size());

    return edges;
}

EdgeList minimum_spanning_tree(const Eigen::MatrixXd& points)
{
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    EdgeList mst_edges;
    mst_edges.reserve(n - 1);

    std::vector<bool> in_mst(n, false);
    std::priority_queue<Edge, std::vector<Edge>, std::greater<>> pq;

    in_mst[0] = true;

    for (Eigen::Index j = 1; j < n; ++j) {
        double dist = distance(points.col(0), points.col(j));
        pq.push({ 0, static_cast<size_t>(j), dist });
    }

    while (!pq.empty() && mst_edges.size() < static_cast<size_t>(n - 1)) {
        Edge e = pq.top();
        pq.pop();

        if (in_mst[e.b])
            continue;

        mst_edges.emplace_back(e.a, e.b);
        in_mst[e.b] = true;

        for (Eigen::Index j = 0; j < n; ++j) {
            if (!in_mst[j]) {
                double dist = distance(points.col(e.b), points.col(j));
                pq.push({ e.b, static_cast<size_t>(j), dist });
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
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    EdgeList edges;

    for (Eigen::Index i = 0; i < n; ++i) {
        for (Eigen::Index j = i + 1; j < n; ++j) {
            Eigen::VectorXd p = points.col(i);
            Eigen::VectorXd q = points.col(j);

            double pq_dist_sq = distance_squared(p, q);
            bool is_gabriel_edge = true;

            for (Eigen::Index k = 0; k < n; ++k) {
                if (k == i || k == j)
                    continue;

                Eigen::VectorXd r = points.col(k);

                double pr_dist_sq = distance_squared(p, r);
                double qr_dist_sq = distance_squared(q, r);

                if (pr_dist_sq + qr_dist_sq < pq_dist_sq) {
                    is_gabriel_edge = false;
                    break;
                }
            }

            if (is_gabriel_edge) {
                edges.emplace_back(static_cast<size_t>(i), static_cast<size_t>(j));
            }
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "gabriel_graph: {} points, generated {} edges",
        n, edges.size());

    return edges;
}

EdgeList nearest_neighbor_graph(const Eigen::MatrixXd& points)
{
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    EdgeList edges;
    edges.reserve(n);

    for (Eigen::Index i = 0; i < n; ++i) {
        double min_dist_sq = std::numeric_limits<double>::max();
        size_t nearest = i;

        for (Eigen::Index j = 0; j < n; ++j) {
            if (i == j)
                continue;

            double dist_sq = distance_squared(points.col(i), points.col(j));
            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                nearest = static_cast<size_t>(j);
            }
        }

        if (nearest != static_cast<size_t>(i)) {
            edges.emplace_back(static_cast<size_t>(i), nearest);
        }
    }

    MF_DEBUG(Journal::Component::Kinesis, Journal::Context::Runtime,
        "nearest_neighbor_graph: {} points, generated {} edges", n, edges.size());

    return edges;
}

EdgeList relative_neighborhood_graph(const Eigen::MatrixXd& points)
{
    Eigen::Index n = points.cols();
    if (n < 2) {
        return {};
    }

    EdgeList edges;

    for (Eigen::Index i = 0; i < n; ++i) {
        for (Eigen::Index j = i + 1; j < n; ++j) {
            Eigen::VectorXd p = points.col(i);
            Eigen::VectorXd q = points.col(j);

            double pq_dist = distance(p, q);
            bool is_rng_edge = true;

            for (Eigen::Index k = 0; k < n; ++k) {
                if (k == i || k == j)
                    continue;

                Eigen::VectorXd r = points.col(k);

                double pr_dist = distance(p, r);
                double qr_dist = distance(q, r);

                double max_dist = std::max(pr_dist, qr_dist);

                if (max_dist < pq_dist) {
                    is_rng_edge = false;
                    break;
                }
            }

            if (is_rng_edge) {
                edges.emplace_back(static_cast<size_t>(i), static_cast<size_t>(j));
            }
        }
    }

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
