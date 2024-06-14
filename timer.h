// only for simplest 'speed tests'
#include <chrono>
#include <iostream>

class Timer {
public:
    Timer() {
        start();
    }

    ~Timer() {
        timePast(stop());
    }
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        return elapsed_time.count();
    }

    void timePast(double elapsed_time) {
        std::cout << "execution time: " << elapsed_time << " ms" << std::endl;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};
