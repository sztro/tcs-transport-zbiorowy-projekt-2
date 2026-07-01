#include "raptor.hpp"
#include "constants.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace gtfs {

std::vector<int> runEarliestArrivalRAPTOR(
    const Network& network, 
    int source_stop_int_id, 
    int target_stop_int_id,
    int start_time_seconds) 
{
    const int num_stops = network.stops.size();
    std::vector<int> earliest_arrival(num_stops, kInfinity);
    
    std::vector<std::vector<TransferConnection>> transfers_from(num_stops);
    for (const auto& transfer : network.transfers) {
        transfers_from[transfer.from_stop_int_id].push_back(transfer);
    }

    // Inicjalizacja
    earliest_arrival[source_stop_int_id] = start_time_seconds;
    std::unordered_set<int> marked_stops;
    marked_stops.insert(source_stop_int_id);
    
    for (const auto& footpath : transfers_from[source_stop_int_id]) {
        earliest_arrival[footpath.to_stop_int_id] = start_time_seconds + footpath.walking_seconds;
        marked_stops.insert(footpath.to_stop_int_id);
    }

    const int MAX_ROUNDS = 5;

    for (int k = 1; k <= MAX_ROUNDS; ++k) {
        if (marked_stops.empty()) break;

        // 1. Zbieramy trasy obsługujące przystanki z poprzedniej rundy
        std::unordered_map<int, int> active_routes;
        for (int stop_id : marked_stops) {
            for (int route_id : network.routes_serving_stop[stop_id]) {
                const auto& route = network.raptor_routes[route_id];
                for (size_t i = 0; i < route.stops.size(); ++i) {
                    if (route.stops[i] == stop_id) {
                        if (active_routes.find(route_id) == active_routes.end() || i < active_routes[route_id]) {
                            active_routes[route_id] = i;
                        }
                        break;
                    }
                }
            }
        }

        marked_stops.clear();

        // 2. Przechodzimy aktywne trasy
        for (const auto& [route_id, hop_on_index] : active_routes) {
            const auto& route = network.raptor_routes[route_id];
            const RaptorTrip* active_trip = nullptr;

            for (size_t i = hop_on_index; i < route.stops.size(); ++i) {
                int current_stop = route.stops[i];

                if (active_trip != nullptr) {
                    int arr_time = active_trip->stop_times[i];
                    if (arr_time < earliest_arrival[current_stop]) {
                        earliest_arrival[current_stop] = arr_time;
                        marked_stops.insert(current_stop);
                    }
                }

                if (earliest_arrival[current_stop] != kInfinity) {
                    for (const auto& trip : route.trips) {
                        if (trip.stop_times[i] >= earliest_arrival[current_stop]) {
                            if (active_trip == nullptr || trip.stop_times[i] < active_trip->stop_times[i]) {
                                active_trip = &trip;
                            }
                            break; 
                        }
                    }
                }
            }
        }

        // 3. Dodajemy przejścia pieszcze
        std::unordered_set<int> footpath_marked_stops;
        for (int stop_id : marked_stops) {
            for (const auto& footpath : transfers_from[stop_id]) {
                int arrival_by_foot = earliest_arrival[stop_id] + footpath.walking_seconds;
                if (arrival_by_foot < earliest_arrival[footpath.to_stop_int_id]) {
                    earliest_arrival[footpath.to_stop_int_id] = arrival_by_foot;
                    footpath_marked_stops.insert(footpath.to_stop_int_id);
                }
            }
        }
        
        marked_stops.insert(footpath_marked_stops.begin(), footpath_marked_stops.end());
    }

    return earliest_arrival;
}
} // namespace gtfs