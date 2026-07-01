#pragma once

#include "gtfs_connections.hpp"

#include <limits>
#include <string>
#include <unordered_map>

namespace gtfs {


std::vector<int> runEarliestArrivalTimeDependentDijkstra(
    const TimeDependentGraph& graph,
    int source_stop_int_id,
    int target_stop_int_id,
    int start_time_seconds
);

}  // namespace gtfs