#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gtfs {

struct Stop {
    std::string stop_id;
    int int_id = -1;
    std::string stop_name;
    double stop_lat = 0.0;
    double stop_lon = 0.0;
};

struct TripStopTime {
    std::string trip_id;
    std::string stop_id;
    int stop_sequence = 0;
    int arrival_seconds = -1;
    int departure_seconds = -1;
};

struct Trip {
    std::string route_id;
    int route_int_id = -1;
    std::string service_id;
    std::string trip_id;
    int int_id = -1;
    std::string headsign;
    std::string block_id;
    std::string shape_id;
};

struct TripSegmentConnection {
    std::string trip_id;
    std::string route_id;
    std::string service_id;
    std::string from_stop_id;
    std::string to_stop_id;
    int departure_seconds = -1;
    int arrival_seconds = -1;
    int travel_seconds = -1;
    int from_stop_int_id = -1;
    int to_stop_int_id = -1;
    int trip_int_id = -1;
};

struct TransferConnection {
    std::string from_stop_id;
    std::string to_stop_id;
    double distance_meters = 0.0;
    int walking_seconds = -1;
    int from_stop_int_id = -1;
    int to_stop_int_id = -1;
};

struct TimeDependentGraphEdge {
    std::uint32_t to = 0;
    int departure_seconds = -1;
    int arrival_seconds = -1;
    int travel_seconds = -1;
    bool is_transfer = false;
};

struct TimeDependentGraph {
    std::vector<std::string> stop_ids;
    std::unordered_map<std::string, std::size_t> stop_index_by_id;
    std::vector<std::size_t> offsets;
    std::vector<TimeDependentGraphEdge> edges;
};

struct Network {
    std::vector<Stop> stops;
    std::vector<Trip> trips;
    std::vector<TripStopTime> stop_times;
    std::vector<TripSegmentConnection> trip_segments;
    std::vector<TransferConnection> transfers;
    TimeDependentGraph time_dependent_graph;
};

struct BuildOptions {
    double walking_speed_mps = 1.4;
    double max_transfer_distance_m = 500.0;
};

class GtfsFeed {
public:
    bool loadFromDirectory(const std::string& directory);

    const std::unordered_map<std::string, Stop>& stopsById() const { return stops_by_id_; }
    const std::unordered_map<std::string, Trip>& tripsById() const { return trips_by_id_; }
    const std::unordered_multimap<std::string, TripStopTime>& stopTimesByTrip() const { return stop_times_by_trip_; }

    Network buildPartialNetwork(const BuildOptions& options) const;

private:
    static std::vector<std::string> readCsvHeaderAndRow(const std::string& line);
    static std::vector<std::string> parseCsvLine(const std::string& line);
    static std::optional<int> parseTimeToSeconds(const std::string& value);
    static double haversineMeters(double lat1, double lon1, double lat2, double lon2);

    void loadStops(const std::string& filePath);
    void loadTrips(const std::string& filePath);
    void loadStopTimes(const std::string& filePath);

    std::vector<Stop> stops_;
    std::vector<Trip> trips_;
    std::vector<TripStopTime> stop_times_;

    std::unordered_map<std::string, Stop> stops_by_id_;
    std::unordered_map<std::string, Trip> trips_by_id_;
    std::unordered_multimap<std::string, TripStopTime> stop_times_by_trip_;
};

std::vector<GtfsFeed> loadFeeds(const std::vector<std::string>& directories);
TimeDependentGraph buildTimeDependentGraph(const Network& network);
Network buildNetwork(const std::vector<std::string>& directories, const BuildOptions& options);

}  // namespace gtfs