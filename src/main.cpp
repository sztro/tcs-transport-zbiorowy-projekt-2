#include "gtfs_connections.hpp"
#include "td_dijkstra.hpp"
#include "csa.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>


namespace {

void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " [data_root] [walking_speed_mps] [max_transfer_distance_m] [algorithm]\n"
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
    const std::filesystem::path data_root = argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("data");
    const double walking_speed_mps = argc >= 3 ? parseDouble(argv[2], 1.4) : 1.4;
    const double max_transfer_distance_m = argc >= 4 ? parseDouble(argv[3], 500.0) : 500.0;
    const std::string algorithm = argc >= 5 ? argv[4] : "td_dijkstra";
    if (algorithm != "td_dijkstra" && algorithm != "csa") {
        std::cerr << "Invalid algorithm specified: " << algorithm << "\n";
        printUsage(argv[0]);
        return 1;
    }
    if (argc > 5) {
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
        std::cout << "TD graph nodes: " << network.time_dependent_graph.stop_ids.size() << "\n";
        std::cout << "TD graph edges: " << network.time_dependent_graph.edges.size() << "\n";

        // === TEST time-dependent Dijkstra ===
        if (!network.stops.empty()) {
            const int num_runs = 5;
            double total_duration_ms = 0.0;
            int start_time = 8 * 3600; // 8:00 rano

            // Inicjalizacja rzeczy do losowania
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<std::size_t> stop_dist(0, network.stops.size() - 1);
            
            std::cout << "\nProbing " << algorithm << " (" << num_runs << " random S->T queries)...\n";
            
            gtfs::Stop last_source;
            gtfs::Stop last_target;
            int last_arrival_time = gtfs::kTimeDependentInfinity;

            for (int i = 0; i < num_runs; ++i) {
                // Losowanie przystanku początkowego i końcowego
                const gtfs::Stop& source = network.stops[stop_dist(gen)];
                const gtfs::Stop& target = network.stops[stop_dist(gen)];
                
                auto start_clock = std::chrono::high_resolution_clock::now();
                auto arrivals = std::unordered_map<std::string, int>();
                if(algorithm == "td_dijkstra") {
                    arrivals = gtfs::runEarliestArrivalTimeDependentDijkstra(network.time_dependent_graph, source.stop_id, target.stop_id, start_time);
                } else if(algorithm == "csa") {
                    arrivals = gtfs::runEarliestArrivalCSA(network, source.stop_id, target.stop_id, start_time);
                }
            
                
                auto end_clock = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end_clock - start_clock;
                
                std::cout << "  Run #" << (i + 1) << ": " << elapsed.count() << " ms\n";
                total_duration_ms += elapsed.count();

                // Zapisujemy dane jeśli to ostatnia iteracja
                if (i == num_runs - 1) {
                    last_source = source;
                    last_target = target;
                    auto it = arrivals.find(target.stop_id);
                    if (it != arrivals.end()) {
                        last_arrival_time = it->second;
                    }
                }
            }
            
            std::cout << "---------------------------------------\n";
            std::cout << "Total time for " << num_runs << " queries: " << total_duration_ms << " ms\n";
            std::cout << "Average time per query: " << (total_duration_ms / num_runs) << " ms\n";

            std::cout << "\n--- Szczegoly ostatniego zapytania ---\n";
            std::cout << "Skad:  " << last_source.stop_name 
                      << " (ID: " << last_source.stop_id << ") "
                      << "[Lat: " << last_source.stop_lat << ", Lon: " << last_source.stop_lon << "]\n";
            std::cout << "Dokad: " << last_target.stop_name 
                      << " (ID: " << last_target.stop_id << ") "
                      << "[Lat: " << last_target.stop_lat << ", Lon: " << last_target.stop_lon << "]\n";
            
            std::cout << "Czas odjazdu: 08:00:00\n";
            std::cout << "Czas dojazdu: ";
            
            if (last_arrival_time == gtfs::kTimeDependentInfinity) {
                std::cout << "BRAK POLACZENIA (Cel nieosiagalny)\n";
            } else {
                int h = (last_arrival_time / 3600) % 24;
                int m = (last_arrival_time / 60) % 60;
                int s = last_arrival_time % 60;
                
                // Ładne formatowanie HH:MM:SS
                std::cout << std::setfill('0') << std::setw(2) << h << ":" 
                          << std::setfill('0') << std::setw(2) << m << ":" 
                          << std::setfill('0') << std::setw(2) << s << "\n";
            }
            std::cout << "---------------------------------------\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to build network: " << error.what() << "\n";
        return 1;
    }

    return 0;
}