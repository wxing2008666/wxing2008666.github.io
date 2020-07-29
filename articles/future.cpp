#include <iostream>       // std::cin, std::cout, std::ios
#include <functional>     // std::ref
#include <thread>         // std::thread
#include <future>         // std::promise, std::future
#include <exception>      // std::exception, std::current_exception

void get_int(std::promise<int>& prom) {
    int x;
    std::cout << "Please, enter an integer value: ";
    std::cin.exceptions (std::ios::failbit);   // throw on failbit
    try {
        std::cin >> x;                         // sets failbit if input is not int
        prom.set_value(x);
    } catch (std::exception&) {
        prom.set_exception(std::current_exception());
    }
}

void print_int(std::future<int>& fut) {
    try {
        int x = fut.get();
        std::cout << "value: " << x << '\n';
    } catch (std::exception& e) {
        std::cout << "[exception caught: " << e.what() << "]\n";
    }
}

void thread_func(std::future<int>& fut) {
    auto x = fut.get();
    std::cout<<"thread func, get:"<<x<<std::endl;
}

int add(int a, int b) {
    std::cout<<"add"<<std::endl;
    return a + b;
}

int main ()
{
    //
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    //std::thread th1(get_int, std::ref(prom));
    //std::thread th2(print_int, std::ref(fut));
    //th1.join();
    //th2.join();
    //
    std::promise<int> prom1;
    std::future<int> fut1 = prom1.get_future();
    std::thread th3(thread_func, std::ref(fut1));
    prom1.set_value(1);
    th3.join();
    //
    std::packaged_task<int(int, int)> task1(add);
    std::future<int> fut2 = task1.get_future();
    std::thread th4(std::move(task1), 1, 2);
    //task1(1, 2);
    int ret1 = fut2.get();
    std::cout<<"add ret:"<<ret1<<std::endl;
    th4.join();
    //
    std::future<int> fut3 = std::async([](int a) {
        std::cout<<"async"<<std::endl;
        return a;
    }, 1);
    int ret2 = fut3.get();
    std::cout<<"async ret:"<<ret2<<std::endl;

    std::future<int> fut4 = std::async(std::launch::async, [](int a) {
        std::cout<<"async launch::async"<<std::endl;
        return a;
    }, 2);
    int ret3 = fut4.get();
    std::cout<<"async launch::async ret:"<<ret3<<std::endl;

    std::future<int> fut5 = std::async(std::launch::deferred, [](int a) {
        std::cout<<"async launch::deferred"<<std::endl;
        return a;
    }, 3);
    int ret4 = fut5.get();
    std::cout<<"async launch::deferred ret:"<<ret4<<std::endl;

    return 0;
}
