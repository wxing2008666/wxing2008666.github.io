#include <thread>
#include <mutex>
#include <condition_variable>

#include <iostream>
#include <functional>
using namespace std;

void thread_func1(int a) {
    cout<<"thread func1:"<<a<<endl;
}

void thread_func2(int a, int b, int c) {
    cout<<"thread func2:"<<a<<","<<b<<","<<c<<endl;
}

class Test
{
public:
    void test_func1(int a) {
        cout<<"Test, test func1:"<<a<<endl;
    }

    static void test_func2(int a) {
        cout<<"Test, test func2:"<<a<<endl;
    }
};

volatile int count = 0;
std::mutex mtx;
void count_add() {
    //std::lock_guard<std::mutex> lokc(mtx);
    std::unique_lock<std::mutex> lock(mtx);
    count++;
}

std::mutex mtx1;
std::condition_variable cv1;
int cargo = 0;
bool is_true() {
    return cargo != 0;
}

void consumer(int n) {
    for (int i = 0; i < n; i++) {
        std::unique_lock<std::mutex> lock(mtx1);
        cv1.wait(lock, is_true);
        cout<<"cargo:"<<cargo<<endl;
        cargo = 0;
    }
}

int main()
{
    std::thread t1(thread_func1, 1);
    std::thread t2([](int a) {
        cout<<"lambda func:"<<a<<endl;
        return a;
    }, 2);
    std::thread t3(Test::test_func2, 3);
    std::thread t4(std::bind(Test::test_func2, 4));
    Test test;
    std::thread t5(std::bind(&Test::test_func1, test, 5));

    std::thread threads[5];
    for (int i = 0; i < 5; ++i) {
        threads[i] = std::thread(count_add);
    }

    std::function<void(int)> func1 = [](int a) {
        cout<<"function func:"<<a<<endl;
    };
    thread t6(func1, 6);

    std::function<void()> func2 = std::bind(thread_func2, 1, 2, 3);
    thread t7(func2);
    std::function<void(int)> func3 = std::bind(thread_func2, 1, std::placeholders::_1, 3);
    thread t8(func3, 5);
    std::function<void(int, int)> func4 = std::bind(thread_func2, 1, std::placeholders::_2, std::placeholders::_1);
    thread t9(func4, 5, 6);
    func4(8, 7);

    std::thread producer_thread(consumer, 10);
    for (int i = 0; i < 10; ++i) {
        while (is_true())
            std::this_thread::yield();
        std::unique_lock<std::mutex> lock(mtx1);
        cargo = i + 1;
        cv1.notify_one();
    }

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    for (auto& t : threads) {
        t.join();
    }

    t6.join();
    t7.join();
    t8.join();
    t9.join();
    producer_thread.join();
    cout<<"count:"<<count<<endl;
    return 0;
}

