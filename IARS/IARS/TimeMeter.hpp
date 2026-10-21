#pragma once

#include <chrono>
#include <vector>

class TimeMeter
{
private:

    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> tp;
public:
    void start() { tp.push_back(std::chrono::high_resolution_clock::now()); }
    long long stop()
    {
        auto t0 = tp.back();
        auto t1 = std::chrono::high_resolution_clock::now();
        auto d = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        tp.pop_back();
        return d;
    }
};
