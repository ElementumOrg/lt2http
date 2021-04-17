#pragma once

#include <iostream>
#include <random>
#include <sstream>
#include <vector>
#include <ctime>

inline int random(int min, int max) // range : [min, max]
{
    static bool first = true;
    if (first) {
        srand(time(nullptr)); // seeding for the first time only!
        first = false;
    }
    return min + rand() % ((max + 1) - min);
}

inline std::string convertToString(double num) {
    std::ostringstream convert;
    convert << num;
    return convert.str();
}

inline double roundOff(double n) {
    double d = n * 100.0;
    int i = d + 0.5;
    d = (float)i / 100.0;
    return d;
}

inline std::string humanize_bytes(size_t size) {
    if (size < 0)
        return convertToString(size);

    static const char *SIZES[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int div = 0;
    size_t rem = 0;

    while (size >= 1024 && div < (sizeof SIZES / sizeof *SIZES)) {
        rem = (size % 1024);
        div++;
        size /= 1024;

        // Check if size is over our available measures
        if (div >= 5)
            break;
    }

    double size_d = (float)size + (float)rem / 1024.0;
    std::string result = convertToString(roundOff(size_d)) + " " + SIZES[div];
    return result;
}

inline int progression_result(int a, double r, int N) {
    // finding the Nth term in:
    // TN = a1 * r^(N-1)
    return int(a * (double)(pow(r, N - 1))); 
}