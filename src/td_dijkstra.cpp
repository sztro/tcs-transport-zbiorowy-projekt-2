#include "td_dijkstra.hpp"

#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace gtfs {

std::unordered_map<std::string, int> runEarliestArrivalTimeDependentDijkstra(
    const TimeDependentGraph& graph,
    const std::string& source_stop_id,
    const std::string& target_stop_id,
    int start_time_seconds)
{
    std::unordered_map<std::string, int> result;
    if (graph.stop_ids.empty()) {
        return result;
    }

    const auto source_it = graph.stop_index_by_id.find(source_stop_id);
    if (source_it == graph.stop_index_by_id.end()) {
        for (const auto& stop_id : graph.stop_ids) {
            if (!stop_id.empty()) {
                result[stop_id] = kTimeDependentInfinity;
            }
        }
        return result;
    }

    const auto target_it = graph.stop_index_by_id.find(target_stop_id);
    const bool has_target = target_it != graph.stop_index_by_id.end();
    const std::size_t source_index = source_it->second;
    const std::size_t target_index = has_target ? target_it->second : graph.stop_ids.size();

    std::vector<int> distance(graph.stop_ids.size(), kTimeDependentInfinity);
    using QueueItem = std::pair<int, std::size_t>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;

    distance[source_index] = start_time_seconds;
    queue.emplace(start_time_seconds, source_index);

    while (!queue.empty()) {
        const auto [current_time, node_index] = queue.top();
        queue.pop();

        if (current_time != distance[node_index]) {
            continue;
        }
        if (has_target && node_index == target_index) {
            break;
        }

        const std::size_t begin = graph.offsets[node_index];
        const std::size_t end = graph.offsets[node_index + 1];
        for (std::size_t edge_index = begin; edge_index < end; ++edge_index) {
            const auto& edge = graph.edges[edge_index];
            const std::size_t to_index = static_cast<std::size_t>(edge.to);

            int candidate_time = kTimeDependentInfinity;
            if (edge.is_transfer) {
                if (edge.travel_seconds < 0) {
                    continue;
                }
                candidate_time = current_time + edge.travel_seconds;
            } else {
                if (edge.departure_seconds < 0 || edge.arrival_seconds < 0) {
                    continue;
                }
                if (current_time > edge.departure_seconds) {
                    continue;
                }
                candidate_time = edge.arrival_seconds;
            }

            if (candidate_time < distance[to_index]) {
                distance[to_index] = candidate_time;
                queue.emplace(candidate_time, to_index);
            }
        }
    }

    for (std::size_t i = 0; i < graph.stop_ids.size(); ++i) {
        const auto& stop_id = graph.stop_ids[i];
        if (!stop_id.empty()) {
            result[stop_id] = distance[i];
        }
    }

    return result;
}

}  // namespace gtfs