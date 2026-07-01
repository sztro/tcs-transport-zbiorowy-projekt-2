#include "gtfs_connections.hpp"
#include "csa.hpp"
#include "constants.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <iostream>

namespace gtfs {

std::vector<int> runEarliestArrivalCSA(
    const Network& network, 
    int source_stop_int_id, 
    int target_stop_int_id,
    int start_time_seconds) 
{
    // S[x] -> najwcześniejszy czas przyjazdu
    std::vector<int> S(network.stops.size(), kInfinity);

    // T[x] -> informacja czy udało nam się już wsiąść na kurs x
    std::vector<bool> T(network.trips.size(), false);

    // Prekalkulacja przesiadek dla szybkiego dostępu po ID przystanku
    std::vector<std::vector<TransferConnection>> transfers_from(network.stops.size());
    for (const auto& transfer : network.transfers) {
        transfers_from[transfer.from_stop_int_id].push_back(transfer);
    }

    // Krok 1 - Setup
    // Inicjalizacja czasu dla przystanku początkowego i jego sąsiadów przy chodzeniu piechotą
    S[source_stop_int_id] = start_time_seconds;
    for (const auto& footpath : transfers_from[source_stop_int_id]) {
        S[footpath.to_stop_int_id] = start_time_seconds + footpath.walking_seconds;
    }

    /// Krok 2: Znalezienie pierwszego połączenia po czasie startowym (binsearch, bo połączenia są posortowane)
    auto start_it = std::lower_bound(
        network.trip_segments.begin(), 
        network.trip_segments.end(), 
        start_time_seconds,
        [](const TripSegmentConnection& connection, int time) {
            return connection.departure_seconds < time;
        }
    );

    // Krok 3 - Główna pętla CSA
    for (auto it = start_it; it != network.trip_segments.end(); ++it) {
        const auto& c = *it;

        // Wszystko dalej już nas nie interesuje, kończymy
        if (S[target_stop_int_id] <= c.departure_seconds) {
            break;
        }

        if (S[c.from_stop_int_id] <= c.departure_seconds || T[c.trip_int_id]) {
            // Connection jest osiągalny, ustawiamy rzeczy
            T[c.trip_int_id] = true;

            if(c.arrival_seconds >= S[c.to_stop_int_id]) {
                continue;
            }

            // Przeglądamy każde przejście piesze z carr_stop
            for (const auto& f : transfers_from[c.to_stop_int_id]) {
                int arrival_by_foot = c.arrival_seconds + f.walking_seconds;
                if (arrival_by_foot < S[f.to_stop_int_id]) {
                    S[f.to_stop_int_id] = arrival_by_foot;
                }
            }
        }
    }

    return S;
}

} // namespace gtfs