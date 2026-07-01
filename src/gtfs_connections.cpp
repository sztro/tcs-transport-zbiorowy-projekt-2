#include "gtfs_connections.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gtfs {

namespace {

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string getField(const std::unordered_map<std::string, std::size_t>& index, const std::vector<std::string>& row, const std::string& name) {
    const auto it = index.find(name);
    if (it == index.end() || it->second >= row.size()) {
        return {};
    }
    return row[it->second];
}

int safeSeconds(const std::optional<int>& value) {
    return value.has_value() ? *value : -1;
}

double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
    constexpr double kEarthRadiusMeters = 6371000.0;
    const double lat1_rad = lat1 * M_PI / 180.0;
    const double lat2_rad = lat2 * M_PI / 180.0;
    const double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    const double delta_lon = (lon2 - lon1) * M_PI / 180.0;

    const double a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
                     std::cos(lat1_rad) * std::cos(lat2_rad) * std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthRadiusMeters * c;
}

std::vector<TransferConnection> buildTransfers(const std::vector<Stop>& stops, const BuildOptions& options) {
    std::vector<TransferConnection> transfers;
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const auto& from = stops[i];
        if (from.stop_id.empty()) {
            continue;
        }
        for (std::size_t j = i + 1; j < stops.size(); ++j) {
            const auto& to = stops[j];
            if (to.stop_id.empty()) {
                continue;
            }
            const double distance = haversineMeters(from.stop_lat, from.stop_lon, to.stop_lat, to.stop_lon);
            if (distance > options.max_transfer_distance_m) {
                continue;
            }
            const auto walking_seconds = static_cast<int>(std::ceil(distance / options.walking_speed_mps));

            TransferConnection forward;
            forward.from_stop_id = from.stop_id;
            forward.to_stop_id = to.stop_id;
            forward.distance_meters = distance;
            forward.walking_seconds = walking_seconds;
            transfers.push_back(forward);

            TransferConnection backward = forward;
            backward.from_stop_id = to.stop_id;
            backward.to_stop_id = from.stop_id;
            transfers.push_back(std::move(backward));
        }
    }
    return transfers;
}

}  // namespace

std::vector<std::string> GtfsFeed::parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                current.push_back(ch);
            }
        } else {
            if (ch == '"') {
                in_quotes = true;
            } else if (ch == ',') {
                fields.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
    }

    fields.push_back(current);
    return fields;
}

std::vector<std::string> GtfsFeed::readCsvHeaderAndRow(const std::string& line) {
    return parseCsvLine(line);
}

std::optional<int> GtfsFeed::parseTimeToSeconds(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    char separator1 = ':';
    char separator2 = ':';
    std::istringstream stream(value);
    if (!(stream >> hours >> separator1 >> minutes >> separator2 >> seconds)) {
        return std::nullopt;
    }
    if (separator1 != ':' || separator2 != ':') {
        return std::nullopt;
    }
    return hours * 3600 + minutes * 60 + seconds;
}

double GtfsFeed::haversineMeters(double lat1, double lon1, double lat2, double lon2) {
    return haversineMeters(lat1, lon1, lat2, lon2);
}

void GtfsFeed::loadStops(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open stops file: " + filePath);
    }

    std::string line;
    if (!std::getline(file, line)) {
        return;
    }
    const auto header = readCsvHeaderAndRow(line);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < header.size(); ++i) {
        index[trim(header[i])] = i;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const auto row = parseCsvLine(line);
        Stop stop;
        stop.stop_id = getField(index, row, "stop_id");
        stop.stop_name = getField(index, row, "stop_name");
        const auto lat = getField(index, row, "stop_lat");
        const auto lon = getField(index, row, "stop_lon");
        if (!lat.empty()) {
            stop.stop_lat = std::stod(lat);
        }
        if (!lon.empty()) {
            stop.stop_lon = std::stod(lon);
        }
        if (!stop.stop_id.empty()) {
            stops_by_id_[stop.stop_id] = stop;
            stops_.push_back(std::move(stop));
        }
    }
}

void GtfsFeed::loadTrips(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open trips file: " + filePath);
    }

    std::string line;
    if (!std::getline(file, line)) {
        return;
    }
    const auto header = readCsvHeaderAndRow(line);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < header.size(); ++i) {
        index[trim(header[i])] = i;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const auto row = parseCsvLine(line);
        Trip trip;
        trip.route_id = getField(index, row, "route_id");
        trip.service_id = getField(index, row, "service_id");
        trip.trip_id = getField(index, row, "trip_id");
        trip.headsign = getField(index, row, "trip_headsign");
        trip.block_id = getField(index, row, "block_id");
        trip.shape_id = getField(index, row, "shape_id");
        if (!trip.trip_id.empty()) {
            trips_by_id_[trip.trip_id] = trip;
            trips_.push_back(std::move(trip));
        }
    }
}

void GtfsFeed::loadStopTimes(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open stop_times file: " + filePath);
    }

    std::string line;
    if (!std::getline(file, line)) {
        return;
    }
    const auto header = readCsvHeaderAndRow(line);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < header.size(); ++i) {
        index[trim(header[i])] = i;
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const auto row = parseCsvLine(line);
        TripStopTime stop_time;
        stop_time.trip_id = getField(index, row, "trip_id");
        stop_time.stop_id = getField(index, row, "stop_id");
        const auto stop_sequence = getField(index, row, "stop_sequence");
        const auto arrival_time = getField(index, row, "arrival_time");
        const auto departure_time = getField(index, row, "departure_time");
        if (!stop_sequence.empty()) {
            stop_time.stop_sequence = std::stoi(stop_sequence);
        }
        stop_time.arrival_seconds = safeSeconds(parseTimeToSeconds(arrival_time));
        stop_time.departure_seconds = safeSeconds(parseTimeToSeconds(departure_time));
        if (!stop_time.trip_id.empty() && !stop_time.stop_id.empty()) {
            stop_times_.push_back(stop_time);
            stop_times_by_trip_.emplace(stop_time.trip_id, stop_time);
        }
    }
}

bool GtfsFeed::loadFromDirectory(const std::string& directory) {
    stops_.clear();
    trips_.clear();
    stop_times_.clear();
    stops_by_id_.clear();
    trips_by_id_.clear();
    stop_times_by_trip_.clear();

    const fs::path dir(directory);
    loadStops((dir / "stops.txt").string());
    loadTrips((dir / "trips.txt").string());
    loadStopTimes((dir / "stop_times.txt").string());
    return true;
}

Network GtfsFeed::buildPartialNetwork(const BuildOptions& options) const {
    Network network;
    network.stops = stops_;
    network.trips = trips_;
    network.stop_times = stop_times_;

    std::unordered_map<std::string, std::vector<TripStopTime>> stop_times_grouped;
    for (const auto& stop_time : stop_times_) {
        stop_times_grouped[stop_time.trip_id].push_back(stop_time);
    }

    for (auto& [trip_id, times] : stop_times_grouped) {
        std::sort(times.begin(), times.end(), [](const TripStopTime& lhs, const TripStopTime& rhs) {
            if (lhs.stop_sequence != rhs.stop_sequence) {
                return lhs.stop_sequence < rhs.stop_sequence;
            }
            return lhs.departure_seconds < rhs.departure_seconds;
        });

        const auto trip_it = trips_by_id_.find(trip_id);
        const Trip* trip = trip_it == trips_by_id_.end() ? nullptr : &trip_it->second;

        for (std::size_t i = 1; i < times.size(); ++i) {
            const auto& from = times[i - 1];
            const auto& to = times[i];
            TripSegmentConnection connection;
            connection.trip_id = trip_id;
            if (trip != nullptr) {
                connection.route_id = trip->route_id;
                connection.service_id = trip->service_id;
            }
            connection.from_stop_id = from.stop_id;
            connection.to_stop_id = to.stop_id;
            connection.departure_seconds = from.departure_seconds >= 0 ? from.departure_seconds : from.arrival_seconds;
            connection.arrival_seconds = to.arrival_seconds >= 0 ? to.arrival_seconds : to.departure_seconds;
            if (connection.departure_seconds >= 0 && connection.arrival_seconds >= 0) {
                connection.travel_seconds = connection.arrival_seconds - connection.departure_seconds;
            }
            network.trip_segments.push_back(std::move(connection));
        }
    }

    return network;
}

std::vector<GtfsFeed> loadFeeds(const std::vector<std::string>& directories) {
    std::vector<GtfsFeed> feeds;
    feeds.reserve(directories.size());
    for (const auto& directory : directories) {
        GtfsFeed feed;
        feed.loadFromDirectory(directory);
        feeds.push_back(std::move(feed));
    }
    return feeds;
}

TimeDependentGraph buildTimeDependentGraph(const Network& network) {
    TimeDependentGraph graph;
    graph.stop_ids.reserve(network.stops.size());
    graph.stop_index_by_id.reserve(network.stops.size());

    for (std::size_t index = 0; index < network.stops.size(); ++index) {
        const auto& stop = network.stops[index];
        graph.stop_ids.push_back(stop.stop_id);
        if (!stop.stop_id.empty()) {
            graph.stop_index_by_id[stop.stop_id] = index;
        }
    }

    struct IndexedEdge {
        std::size_t from = 0;
        TimeDependentGraphEdge edge;
    };

    std::vector<IndexedEdge> indexed_edges;
    indexed_edges.reserve(network.trip_segments.size() + network.transfers.size());

    for (const auto& connection : network.trip_segments) {
        const auto from_it = graph.stop_index_by_id.find(connection.from_stop_id);
        const auto to_it = graph.stop_index_by_id.find(connection.to_stop_id);
        if (from_it == graph.stop_index_by_id.end() || to_it == graph.stop_index_by_id.end()) {
            continue;
        }
        if (connection.departure_seconds < 0 || connection.arrival_seconds < 0) {
            continue;
        }

        IndexedEdge edge;
        edge.from = from_it->second;
        edge.edge.to = static_cast<std::uint32_t>(to_it->second);
        edge.edge.departure_seconds = connection.departure_seconds;
        edge.edge.arrival_seconds = connection.arrival_seconds;
        edge.edge.travel_seconds = connection.travel_seconds;
        edge.edge.is_transfer = false;
        indexed_edges.push_back(std::move(edge));
    }

    for (const auto& transfer : network.transfers) {
        const auto from_it = graph.stop_index_by_id.find(transfer.from_stop_id);
        const auto to_it = graph.stop_index_by_id.find(transfer.to_stop_id);
        if (from_it == graph.stop_index_by_id.end() || to_it == graph.stop_index_by_id.end()) {
            continue;
        }
        if (transfer.walking_seconds < 0) {
            continue;
        }

        IndexedEdge edge;
        edge.from = from_it->second;
        edge.edge.to = static_cast<std::uint32_t>(to_it->second);
        edge.edge.travel_seconds = transfer.walking_seconds;
        edge.edge.is_transfer = true;
        indexed_edges.push_back(std::move(edge));
    }

    std::sort(indexed_edges.begin(), indexed_edges.end(), [](const IndexedEdge& lhs, const IndexedEdge& rhs) {
        if (lhs.from != rhs.from) {
            return lhs.from < rhs.from;
        }
        if (lhs.edge.is_transfer != rhs.edge.is_transfer) {
            return lhs.edge.is_transfer < rhs.edge.is_transfer;
        }
        if (lhs.edge.departure_seconds != rhs.edge.departure_seconds) {
            return lhs.edge.departure_seconds < rhs.edge.departure_seconds;
        }
        return lhs.edge.to < rhs.edge.to;
    });

    graph.offsets.assign(graph.stop_ids.size() + 1, 0);
    graph.edges.reserve(indexed_edges.size());

    std::size_t current_from = 0;
    for (const auto& edge : indexed_edges) {
        while (current_from <= edge.from && current_from < graph.offsets.size()) {
            graph.offsets[current_from] = graph.edges.size();
            ++current_from;
        }
        graph.edges.push_back(edge.edge);
    }
    while (current_from < graph.offsets.size()) {
        graph.offsets[current_from] = graph.edges.size();
        ++current_from;
    }

    return graph;
}
Network buildNetwork(const std::vector<std::string>& directories, const BuildOptions& options) {
    const auto feeds = loadFeeds(directories);
    Network network;
    std::unordered_map<std::string, Stop> merged_stops_by_id;
    std::unordered_map<std::string, Trip> merged_trips_by_id;
    std::vector<TripStopTime> merged_stop_times;

    for (const auto& feed : feeds) {
        const auto partial = feed.buildPartialNetwork(options);
        network.trip_segments.insert(network.trip_segments.end(), partial.trip_segments.begin(), partial.trip_segments.end());

        for (const auto& stop : partial.stops) {
            if (!stop.stop_id.empty()) {
                merged_stops_by_id[stop.stop_id] = stop;
            }
        }
        for (const auto& trip : partial.trips) {
            if (!trip.trip_id.empty()) {
                merged_trips_by_id[trip.trip_id] = trip;
            }
        }
        merged_stop_times.insert(merged_stop_times.end(), partial.stop_times.begin(), partial.stop_times.end());
    }

    network.stops.reserve(merged_stops_by_id.size());
    for (const auto& [stop_id, stop] : merged_stops_by_id) {
        (void)stop_id;
        network.stops.push_back(stop);
    }

    network.trips.reserve(merged_trips_by_id.size());
    for (const auto& [trip_id, trip] : merged_trips_by_id) {
        (void)trip_id;
        network.trips.push_back(trip);
    }

    network.stop_times = std::move(merged_stop_times);
    network.transfers = buildTransfers(network.stops, options);

    // Mapowanie rzeczy na int'y
    std::unordered_map<std::string, int> stop_string_to_int;
    for (std::size_t i = 0; i < network.stops.size(); ++i) {
        network.stops[i].int_id = static_cast<int>(i);
        stop_string_to_int[network.stops[i].stop_id] = static_cast<int>(i);
    }

    std::unordered_map<std::string, int> trip_string_to_int;
    for (std::size_t i = 0; i < network.trips.size(); ++i) {
        network.trips[i].int_id = static_cast<int>(i);
        trip_string_to_int[network.trips[i].trip_id] = static_cast<int>(i);
    }

    // Przydzielenie tychże int'ów
    for (auto& seg : network.trip_segments) {
        seg.from_stop_int_id = stop_string_to_int[seg.from_stop_id];
        seg.to_stop_int_id = stop_string_to_int[seg.to_stop_id];
        seg.trip_int_id = trip_string_to_int[seg.trip_id];
    }

    for (auto& trans : network.transfers) {
        trans.from_stop_int_id = stop_string_to_int[trans.from_stop_id];
        trans.to_stop_int_id = stop_string_to_int[trans.to_stop_id];
    }
    
    network.time_dependent_graph = buildTimeDependentGraph(network);


    // ==========================================
    // RZECZY DO CSA
    // ==========================================
    // Posortuj połączenia
    std::sort(network.trip_segments.begin(), network.trip_segments.end(),
        [](const TripSegmentConnection& a, const TripSegmentConnection& b) {
            return a.departure_seconds < b.departure_seconds;
        }
    );


    // ==========================================
    // RZECZY DO RAPTORA
    // ==========================================

    // 1. Grupujemy segmenty po trip_int_id
    std::unordered_map<int, std::vector<TripSegmentConnection>> segments_by_trip;
    for (const auto& seg : network.trip_segments) {
        segments_by_trip[seg.trip_int_id].push_back(seg);
    }

    // 2. Znajdujemy unikalne stop sequences / trasy
    std::unordered_map<std::string, RaptorRoute> route_map;
    int next_route_id = 0;

    for (auto& [trip_id, segments] : segments_by_trip) {
        std::sort(segments.begin(), segments.end(), [](const auto& a, const auto& b) {
            return a.departure_seconds < b.departure_seconds;
        });

        std::vector<int> stop_sequence;
        std::vector<int> stop_times;

        stop_sequence.push_back(segments.front().from_stop_int_id);
        stop_times.push_back(segments.front().departure_seconds);

        for (const auto& seg : segments) {
            stop_sequence.push_back(seg.to_stop_int_id);
            stop_times.push_back(seg.arrival_seconds);
        }

        std::string sequence_key = "";
        for (int stop : stop_sequence) {
            sequence_key += std::to_string(stop) + "-";
        }

        if (route_map.find(sequence_key) == route_map.end()) {
            RaptorRoute new_route;
            new_route.route_int_id = next_route_id++;
            new_route.stops = stop_sequence;
            route_map[sequence_key] = new_route;
        }

        RaptorTrip r_trip;
        r_trip.trip_int_id = trip_id;
        r_trip.stop_times = stop_times;
        route_map[sequence_key].trips.push_back(r_trip);
    }

    // 3. Dodajemy trasy do network, potem sortujemy kursy w ich obrębie
    network.raptor_routes.reserve(route_map.size());
    for (auto& [key, route] : route_map) {
        std::sort(route.trips.begin(), route.trips.end(), [](const RaptorTrip& a, const RaptorTrip& b) {
            return a.stop_times[0] < b.stop_times[0];
        });
        
        route.route_int_id = network.raptor_routes.size(); 
        network.raptor_routes.push_back(std::move(route));
    }

    // 4. Uzupełniamy routes_serving_stop
    network.routes_serving_stop.resize(network.stops.size());
    for (const auto& route : network.raptor_routes) {
        for (int stop_id : route.stops) {
            auto& served = network.routes_serving_stop[stop_id];
            if (std::find(served.begin(), served.end(), route.route_int_id) == served.end()) {
                served.push_back(route.route_int_id);
            }
        }
    }

    return network;
}

}  // namespace gtfs