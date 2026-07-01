#pragma once

#include "gtfs_connections.hpp"
#include <string>
#include <unordered_map>
#include <limits>

namespace gtfs {

// Guard / stała reprezentująca nieskończoność / nieosiągalny przystanek

std::unordered_map<std::string, int> runEarliestArrivalCSA(
    const Network& network, 
    const std::string& source_stop_id, 
    const std::string& target_stop_id,
    int start_time_seconds
);

} // namespace gtfs