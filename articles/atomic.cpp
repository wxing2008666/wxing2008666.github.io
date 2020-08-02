#include <atomic>
#include <thread>

#include <vector>
#include <iostream>
using namespace std;

std::atomic_flag flag_lock = ATOMIC_FLAG_INIT;

std::atomic<bool> ready{false};
std::atomic_flag winner = ATOMIC_FLAG_INIT;

void func1(int n) {
    for (int i = 0; i < 10; ++i) {
        while (flag_lock.test_and_set(std::memory_order_acquire)); //acquire lock, spin
        cout<<"output from thread:"<<n<<endl;
        flag_lock.clear(std::memory_order_release);
    }
}

void func2(int n) {
    while (!ready)
        std::this_thread::yield();

    for (volatile int i = 0; i < 10000; ++i);

    if (!winner.test_and_set()) {
        cout<<"thread:"<<n<<endl;
    }
}

int main()
{
    vector<thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(func1, i);
    }

    for (auto& t : threads)
        t.join();

    vector<thread> vt;
    for (int i = 0; i < 10; ++i) {
        vt.emplace_back(func2, i);
    }
    ready = true;

    for (auto& t : vt)
        t.join();

    return 0;
}
