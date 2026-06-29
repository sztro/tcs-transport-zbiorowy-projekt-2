#include "gtfs_connections.hpp"
#include "csa.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <iostream>

namespace gtfs {

std::unordered_map<std::string, int> runEarliestArrivalCSA(
    const Network& network, 
    const std::string& source_stop_id, 
    int start_time_seconds) 
{
    // S[x] -> najwcześniejszy czas przyjazdu
    std::unordered_map<std::string, int> S;
    for (const auto& stop : network.stops) {
        S[stop.stop_id] = kInfinity;
    }

    // T[x] -> informacja czy udało nam się już wsiąść na kurs x
    std::unordered_map<std::string, bool> T;
    for (const auto& trip : network.trips) {
        T[trip.trip_id] = false;
    }

    // Prekalkulacja przesiadek dla szybkiego dostępu po ID przystanku
    std::unordered_map<std::string, std::vector<TransferConnection>> transfers_from;
    for (const auto& transfer : network.transfers) {
        transfers_from[transfer.from_stop_id].push_back(transfer);
    }

    // Krok 1 - Setup
    // Inicjalizacja czasu dla przystanku początkowego i jego sąsiadów przy chodzeniu piechotą
    S[source_stop_id] = start_time_seconds;
    if (auto it = transfers_from.find(source_stop_id); it != transfers_from.end()) {
        for (const auto& footpath : it->second) {
            S[footpath.to_stop_id] = start_time_seconds + footpath.walking_seconds;
        }
    }

    // Krok 2 - Setup c.d.
    // Pobranie i posortowanie wszystkich połączeń po cdep_time (departure_seconds)
    // W zoptymalizowanej wersji to sortowanie powinno się odbywać raz, podczas budowy sieci.
    std::vector<TripSegmentConnection> connections = network.trip_segments;
    std::sort(connections.begin(), connections.end(), 
        [](const TripSegmentConnection& a, const TripSegmentConnection& b) {
            return a.departure_seconds < b.departure_seconds;
        }
    );

    // Krok 3 - Główna pętla CSA
    for (const auto& c : connections) {
        // Ignorujemy połączenia z przeszłości
        if (c.departure_seconds < start_time_seconds) {
            continue;
        }

        if (S[c.from_stop_id] <= c.departure_seconds || T[c.trip_id]) {
            // Connection jest osiągalny, ustawiamy rzeczy
            T[c.trip_id] = true;

            // Przeglądamy każde przejście piesze z carr_stop
            
            if (auto it = transfers_from.find(c.to_stop_id); it != transfers_from.end()) {
                for (const auto& f : it->second) {
                    int arrival_by_foot = c.arrival_seconds + f.walking_seconds;
                    // S[farr_stop] = min{S[farr_stop], carr_time + fdur}
                    if (arrival_by_foot < S[f.to_stop_id]) {
                        S[f.to_stop_id] = arrival_by_foot;
                    }
                }
            }
        }
    }

    return S;
}

} // namespace gtfs