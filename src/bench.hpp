#pragma once

#include <string>
#include <vector>
#include <functional>

namespace gtfs {

struct RoutingQuery {
    int source_id;
    int target_id;
    int start_time;
};

struct BenchmarkResult {
    std::string name;
    double total_ms = 0;
    double mean_ms = 0;
    double p50_ms = 0;
    double p95_ms = 0;
    double p99_ms = 0;
    double max_ms = 0;
};

BenchmarkResult runBenchmark(
    const std::string& name,
    const std::vector<RoutingQuery>& queries,
    const std::function<void(const RoutingQuery&)>& algo_func
);

void printResult(const BenchmarkResult& r);

} // namespace gtfs