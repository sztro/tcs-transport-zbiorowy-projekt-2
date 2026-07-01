#include "bench.hpp"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace gtfs {

BenchmarkResult runBenchmark(
    const std::string& name,
    const std::vector<RoutingQuery>& queries,
    const std::function<void(const RoutingQuery&)>& algo_func) 
{
    std::vector<double> durations;
    durations.reserve(queries.size());

    // Liczymy bez mierzenia czasu, by "rozruszać" CPU przed właściwym benchmarkiem 
    int warmup_count = std::max(1, static_cast<int>(queries.size() * 0.1));
    for (int i = 0; i < warmup_count; ++i) {
        algo_func(queries[i]);
    }

    double total_time = 0.0;
    for (const auto& q : queries) {
        auto start = std::chrono::high_resolution_clock::now();
        
        algo_func(q);
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        durations.push_back(elapsed.count());
        total_time += elapsed.count();
    }

    // Liczymy statystyki
    std::sort(durations.begin(), durations.end());
    
    BenchmarkResult res;
    res.name = name;
    res.total_ms = total_time;
    res.mean_ms = total_time / durations.size();
    res.p50_ms = durations[durations.size() * 0.50];
    res.p95_ms = durations[durations.size() * 0.95];
    res.p99_ms = durations[durations.size() * 0.99];
    res.max_ms = durations.back();

    return res;
}

void printResult(const BenchmarkResult& r) {
    std::cout << std::left << std::setw(15) << r.name 
              << "| Mean: " << std::setw(8) << r.mean_ms 
              << "| P50: " << std::setw(8) << r.p50_ms 
              << "| P95: " << std::setw(8) << r.p95_ms 
              << "| P99: " << std::setw(8) << r.p99_ms 
              << "| Max: " << r.max_ms << " ms\n";
}

} // namespace gtfs