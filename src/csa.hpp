#pragma once

#include "gtfs_connections.hpp"
#include <string>
#include <unordered_map>
#include <limits>

namespace gtfs {


std::vector<int> runEarliestArrivalCSA(
    const Network& network, 
    int source_stop_id, 
    int target_stop_id,
    int start_time_seconds
);

} // namespace gtfs