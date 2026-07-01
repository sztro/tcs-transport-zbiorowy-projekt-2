#include "td_dijkstra.hpp"
#include "constants.hpp"

#include <functional>
#include <queue>
#include <utility>
#include <vector>
#include <cassert>

namespace gtfs {

std::vector<int> runEarliestArrivalTimeDependentDijkstra(
    const TimeDependentGraph& graph,
    int source_stop_int_id,
    int target_stop_int_id,
    int start_time_seconds)
{

    const int num_nodes = graph.stop_ids.size();
    std::vector<int> distance(num_nodes, kInfinity);

    if (source_stop_int_id < 0 || source_stop_int_id >= num_nodes) {
        assert(false && "Error: Bledne zrodlo dla TD-Dijsktry");
        return distance;
    }

    const bool has_target = target_stop_int_id >= 0 && target_stop_int_id < num_nodes;
    const int source_index = source_stop_int_id;
    const int target_index = has_target ? target_stop_int_id : num_nodes;

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
        if (has_target && node_index == static_cast<std::size_t>(target_index)) {
            break;
        }

        const std::size_t begin = graph.offsets[node_index];
        const std::size_t end = graph.offsets[node_index + 1];
        for (std::size_t edge_index = begin; edge_index < end; ++edge_index) {
            const auto& edge = graph.edges[edge_index];
            const std::size_t to_index = static_cast<std::size_t>(edge.to);

            int candidate_time = kInfinity;
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

    return distance;
}

}  // namespace gtfs