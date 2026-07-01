#pragma once

#include "gtfs_connections.hpp"
#include <vector>

namespace gtfs {

std::vector<int> runEarliestArrivalRAPTOR(
    const Network& network, 
    int source_stop_int_id, 
    int target_stop_int_id,
    int start_time_seconds
);

} // namespace gtfs