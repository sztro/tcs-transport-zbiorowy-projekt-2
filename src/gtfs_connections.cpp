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

    // Posortuj połączenia (do CSA)
    std::sort(network.trip_segments.begin(), network.trip_segments.end(),
        [](const TripSegmentConnection& a, const TripSegmentConnection& b) {
            return a.departure_seconds < b.departure_seconds;
        }
    );

    return network;
}

}  // namespace gtfs