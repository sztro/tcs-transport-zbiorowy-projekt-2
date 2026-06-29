#pragma once

#include "gtfs_connections.hpp"
#include <string>
#include <unordered_map>
#include <limits>

namespace gtfs {

// Guard / stała reprezentująca nieskończoność / nieosiągalny przystanek
constexpr int kInfinity = std::numeric_limits<int>::max();

std::unordered_map<std::string, int> runEarliestArrivalCSA(
    const Network& network, 
    const std::string& source_stop_id, 
    int start_time_seconds
);

} // namespace gtfs