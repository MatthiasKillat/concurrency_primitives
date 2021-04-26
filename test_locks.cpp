#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <stdint.h>
#include <atomic>

#include "lock.hpp"
#include "id_lock.hpp"
#include "mutex.hpp"

struct NoLock
{
    void lock() {}

    void unlock() {}
};

int64_t count{0};

//track the number of users in a (reduced) part of the critical section
//if it is anything other then 1 (after increment), we count a mutex error
std::atomic<int> users{0};
std::atomic<uint64_t> mutexError{0};

template <typename LockType>
void work(LockType &lock, int a = 1, int iterations = 1000000)
{
    for (int i = 0; i < iterations; ++i)
    {
        lock.lock();

        if (users.fetch_add(1, std::memory_order_acq_rel) != 0)
        {
            mutexError.fetch_add(1);
        }

        count += a;

        users.fetch_sub(1, std::memory_order_release);
        lock.unlock();
    }
}

template <typename LockType>
void test(int iterations = 1000000, int n = 4)
{
    LockType lock;

    std::vector<std::thread> threads;
    threads.reserve(2 * n);

    count = 0;
    users.store(0, std::memory_order_relaxed);
    mutexError.store(0, std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        threads.emplace_back(work<LockType>, std::ref(lock), 1, iterations);
        threads.emplace_back(work<LockType>, std::ref(lock), -1, iterations);
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

    // {
    //     auto start = std::chrono::high_resolution_clock::now();
    //     test<NoLock>(iterations, n);
    //     auto end = std::chrono::high_resolution_clock::now();
    //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    //     std::cout << "NoLock test: count " << count << " mutex errors " << mutexError.load() << " time " << elapsed.count() << "ms" << std::endl;
    // }

    // {
    //     auto start = std::chrono::high_resolution_clock::now();
    //     test<std::mutex>(iterations, n);
    //     auto end = std::chrono::high_resolution_clock::now();
    //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    //     std::cout << "std::mutex test: count " << count << " mutex errors " << mutexError.load() << " time " << elapsed.count() << "ms" << std::endl;
    // }

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<Lock>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Lock test: count " << count << " mutex errors " << mutexError.load() << " time " << elapsed.count() << "ms" << std::endl;
    }
    //
    // {
    //     auto start = std::chrono::high_resolution_clock::now();
    //     test<Mutex>(iterations, n);
    //     auto end = std::chrono::high_resolution_clock::now();
    //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    //     std::cout << "Mutex: count " << count << " mutex errors " << mutexError.load() << " time " << elapsed.count() << "ms" << std::endl;
    // }

    {
        auto start = std::chrono::high_resolution_clock::now();
        test<IdLock>(iterations, n);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "IdLock test: count " << count << " mutex errors " << mutexError.load() << " time " << elapsed.count() << "ms" << std::endl;
    }

    return 0;
}