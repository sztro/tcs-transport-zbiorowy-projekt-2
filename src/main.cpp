#include "gtfs_connections.hpp"
#include "csa.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " [data_root] [walking_speed_mps] [max_transfer_distance_m]\n"
              << "Defaults: data_root=./data, walking_speed_mps=1.4, max_transfer_distance_m=500\n";
}

double parseDouble(const char* text, double fallback) {
    if (text == nullptr || *text == '\0') {
        return fallback;
    }
    try {
        return std::stod(text);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path data_root = argc >= 2 ? fs::path(argv[1]) : fs::path("data");
    const double walking_speed_mps = argc >= 3 ? parseDouble(argv[2], 1.4) : 1.4;
    const double max_transfer_distance_m = argc >= 4 ? parseDouble(argv[3], 500.0) : 500.0;

    if (argc > 4) {
        printUsage(argv[0]);
        return 1;
    }

    const std::vector<std::string> feeds = {
        (data_root / "GTFS_KRK_A").string(),
        (data_root / "GTFS_KRK_T").string(),
        (data_root / "GTFS_KRK_M").string(),
    };

    gtfs::BuildOptions options;
    options.walking_speed_mps = walking_speed_mps;
    options.max_transfer_distance_m = max_transfer_distance_m;

    try {
        const gtfs::Network network = gtfs::buildNetwork(feeds, options);

        std::cout << "Loaded Krakow GTFS network from " << data_root << "\n";
        std::cout << "Stops: " << network.stops.size() << "\n";
        std::cout << "Trips: " << network.trips.size() << "\n";
        std::cout << "Stop times: " << network.stop_times.size() << "\n";
        std::cout << "Trip segments: " << network.trip_segments.size() << "\n";
        std::cout << "Transfers: " << network.transfers.size() << "\n";

        // === TEST CSA ===
        // bierzemy pierwszy lepszy przystanek z sieci jako startowy
        if (!network.stops.empty()) {
            std::string start_stop = network.stops.front().stop_id;
            int start_time = 8 * 3600; // 8:00 rano
            
            std::cout << "\nRunning CSA from stop " << start_stop << " at 8:00 AM...\n";
            auto arrivals = gtfs::runEarliestArrivalCSA(network, start_stop, start_time);
            
            int reachable_stops = 0;
            for (const auto& [stop_id, arrival_time] : arrivals) {
                if (arrival_time != gtfs::kInfinity) {
                    reachable_stops++;
                }
            }
            std::cout << "Reachable stops: " << reachable_stops << " / " << network.stops.size() << "\n";
        }
        // ----------------------
    } catch (const std::exception& error) {
        std::cerr << "Failed to build network: " << error.what() << "\n";
        return 1;
    }

    return 0;
}