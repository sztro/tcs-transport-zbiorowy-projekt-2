#include "gtfs_connections.hpp"
#include "td_dijkstra.hpp"
#include "csa.hpp"
#include "raptor.hpp"
#include "constants.hpp"
#include "bench.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>


namespace {

void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " [data_root] ] [algorithm]\n"
              << "Defaults: data_root=./data, algorithm=td_dijkstra\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path data_root = argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("data");
    if (argc > 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::vector<std::string> feeds = {
        (data_root / "GTFS_KRK_A").string(),
        (data_root / "GTFS_KRK_T").string(),
        (data_root / "GTFS_KRK_M").string(),
    };

    gtfs::BuildOptions options;
    options.walking_speed_mps = gtfs::kWalkingSpeedMps;
    options.max_transfer_distance_m = gtfs::kMaxTransferDistanceMeters;

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

        // =================
        // === BENCHMARK ===
        // =================
        if (!network.stops.empty()) {
            const int num_runs = 1000;
            int start_time = 8 * 3600; // 8:00

            std::cout << "\nGenerating " << num_runs << " S->T queries...\n";
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<std::size_t> stop_dist(0, network.stops.size() - 1);
            std::uniform_int_distribution<int> time_dist(start_time - 3600, start_time + 3600); 

            std::vector<gtfs::RoutingQuery> queries;
            queries.reserve(num_runs);
            for (int i = 0; i < num_runs; ++i) {
                queries.push_back({
                    network.stops[stop_dist(gen)].int_id,
                    network.stops[stop_dist(gen)].int_id,
                    time_dist(gen) 
                });
            }

            std::cout << "Starting benchmarks...\n";
            std::cout << std::string(80, '-') << "\n";

            // 1. TD-Dijkstra
            auto res_dijkstra = gtfs::runBenchmark("TD-Dijkstra", queries, [&](const gtfs::RoutingQuery& q) {
                gtfs::runEarliestArrivalTimeDependentDijkstra(
                    network.time_dependent_graph, q.source_id, q.target_id, q.start_time);
            });
            gtfs::printResult(res_dijkstra);

            // 2. CSA
            auto res_csa = gtfs::runBenchmark("CSA", queries, [&](const gtfs::RoutingQuery& q) {
                gtfs::runEarliestArrivalCSA(
                    network, q.source_id, q.target_id, q.start_time);
            });
            gtfs::printResult(res_csa);

            // 3. RAPTOR
            auto res_raptor = gtfs::runBenchmark("RAPTOR", queries, [&](const gtfs::RoutingQuery& q) {
                gtfs::runEarliestArrivalRAPTOR(
                    network, q.source_id, q.target_id, q.start_time);
            });
            gtfs::printResult(res_raptor);

            std::cout << std::string(80, '-') << "\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to build network: " << error.what() << "\n";
        return 1;
    }

    return 0;
}