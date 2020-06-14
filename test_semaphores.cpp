#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <stdint.h>

#include "semaphore.hpp"
#include "posix_semaphore.hpp"
#include "lightweight_semphore.hpp"

using LightSemaphore = LightweightSemaphore<Semaphore>;
using LightPosixSemaphore = LightweightSemaphore<PosixSemaphore>;

template <typename SemaphoreType>
void wait(SemaphoreType &semaphore, int iterations = 1000000)
{
    for (int i = 0; i < iterations; ++i)
    {
        semaphore.wait();
    }
}

template <typename SemaphoreType>
void signal(SemaphoreType &semaphore, int iterations = 1000000)
{
    for (int i = 0; i < iterations; ++i)
    {
        semaphore.post();
    }
}

template <typename SemaphoreType>
void test(int iterations = 1000000, int n = 4)
{
    SemaphoreType semaphore;

    std::vector<std::thread> threads;
    threads.reserve(2 * n);

    for (int i = 0; i < n; ++i)
    {
        threads.emplace_back(wait<SemaphoreType>, std::ref(semaphore), iterations);
        threads.emplace_back(signal<SemaphoreType>, std::ref(semaphore), iterations);
    }

    for (auto &thread : threads)
    {
        thread.join();
    }
}

int main(int argc, char **argv)
{
    int iterations = 1000000;
    int n = 8;

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<Semaphore>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Semaphore test: time " << elapsed.count() << "ms" << std::endl;
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<PosixSemaphore>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "PosixSemaphore test: time " << elapsed.count() << "ms" << std::endl;
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<LightSemaphore>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "LightSemaphore test: time " << elapsed.count() << "ms" << std::endl;
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<LightPosixSemaphore>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "LightPosixSemaphore test: time " << elapsed.count() << "ms" << std::endl;
    }

    return 0;
}