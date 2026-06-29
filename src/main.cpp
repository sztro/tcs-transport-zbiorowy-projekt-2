#include "gtfs_connections.hpp"
#include "csa.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>

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
        if (!network.stops.empty()) {
            std::string start_stop = network.stops.front().stop_id;
            int start_time = 8 * 3600; // 8:00 rano
            
            const int num_runs = 5; // Liczba prób
            double total_duration_ms = 0.0;
            
            std::cout << "\nProbing CSA performance (" << num_runs << " runs) from stop " << start_stop << "...\n";
            
            for (int i = 0; i < num_runs; ++i) {
                auto start_clock = std::chrono::high_resolution_clock::now();
                
                auto arrivals = gtfs::runEarliestArrivalCSA(network, start_stop, start_time);
                
                auto end_clock = std::chrono::high_resolution_clock::now();
                
                std::chrono::duration<double, std::milli> elapsed = end_clock - start_clock;
                
                std::cout << "  Run #" << (i + 1) << ": " << elapsed.count() << " ms\n";
                total_duration_ms += elapsed.count();
            }
            
            std::cout << "---------------------------------------\n";
            std::cout << "Total time for " << num_runs << " queries: " << total_duration_ms << " ms\n";
            std::cout << "Average time per query: " << (total_duration_ms / num_runs) << " ms\n";
        }
        // ----------------------
    } catch (const std::exception& error) {
        std::cerr << "Failed to build network: " << error.what() << "\n";
        return 1;
    }

    return 0;
}