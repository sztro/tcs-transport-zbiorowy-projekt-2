#pragma once

#include "gtfs_connections.hpp"

#include <limits>
#include <string>
#include <unordered_map>

namespace gtfs {

inline constexpr int kTimeDependentInfinity = std::numeric_limits<int>::max();

std::unordered_map<std::string, int> runEarliestArrivalTimeDependentDijkstra(
    const TimeDependentGraph& graph,
    const std::string& source_stop_id,
    const std::string& target_stop_id,
    int start_time_seconds
);

}  // namespace gtfs