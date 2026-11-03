#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <string>

// Random number in [min, max].
inline int random(int min, int max) {
    thread_local std::mt19937 engine{std::random_device{}()};
    return std::uniform_int_distribution<int>(min, max)(engine);
}

// Size with a unit, rounded to 2 decimals: "1.5 MB".
inline std::string humanize_bytes(std::int64_t size) {
    if (size < 0)
        return std::to_string(size);

    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = double(size);
    int unit = 0;
    while (value >= 1024 && unit < 5) {
        value /= 1024;
        unit++;
    }

    char result[32];
    snprintf(result, sizeof(result), "%g %s", std::round(value * 100) / 100, units[unit]);
    return result;
}

// Nth term of a geometric progression: a * r^(N-1).
inline int progression_result(int a, double r, int N) { return int(a * std::pow(r, N - 1)); }
