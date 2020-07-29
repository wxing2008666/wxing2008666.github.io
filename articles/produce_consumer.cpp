#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <condition_variable>

#include <vector>
#include <queue>
#include <functional>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace std;

class ThreadPoolSimple
{
public:
    using Task = std::function<void()>;

    ThreadPoolSimple(int thread_num) {
        for (int i = 0; i < thread_num; ++i) {
            pool_.emplace_back(std::thread(&ThreadPoolSimple::run, this));
        }
    }

    ~ThreadPoolSimple() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            is_running_ = false;
            cv_.notify_all();
        }
        for (thread& t : pool_) {
            if (t.joinable())
                t.join();
        }
    }

    void commit(Task task) {
        if (is_running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push(task);
            cv_.notify_one();
        } else {
            cout<<"ThreadPool is not running"<<endl;
        }
    }

    void run() {
        while (is_running_) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                /*
                if (!tasks_.empty()) {
                    task = tasks_.front();
                    tasks_.pop();
                } else if (is_running_ && tasks_.empty()) {
                    cv_.wait(lock);
                }
                */
               cv_.wait(lock, [this]{
                   return !is_running_ || !tasks_.empty();
               });

               if (!is_running_ && tasks_.empty())
                   return;

               task = std::move(tasks_.front());
               tasks_.pop();
            }
            if (task)
                task();
        }
    }
private:
    std::vector<std::thread> pool_;
    std::queue<Task>         tasks_;
    std::mutex               mutex_;
    std::condition_variable  cv_;
    std::atomic<bool>        is_running_{true};
};

class ThreadPool
{
public:
    using Task = std::function<void()>;
    unsigned short THREADPOOL_MAX_NUM{16};

public:
    inline ThreadPool(unsigned short size = 4) {addThread(size);}
    inline ~ThreadPool()
    {
        is_running_ = false;
        task_cv_.notify_all();

        for (thread& t : pool_) {
            if (t.joinable())
                t.join();
        }
    }

public:
    void addThread(unsigned short size) {
        for (; pool_.size() < THREADPOOL_MAX_NUM && size > 0; --size) {
            pool_.emplace_back([this]{
                while (is_running_) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        task_cv_.wait(lock, [this]{
                            return !is_running_ || !tasks_.empty();
                        });

                        if (!is_running_ && tasks_.empty())
                            return;

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    if (task) {
                        idle_thread_num_--;
                        task();
                        idle_thread_num_++;
                    }
                }
            });
            idle_thread_num_++;
        }
    }

    template<typename F, typename... Args>
    auto commit(F&& f, Args&&... args) ->std::future<decltype(f(args...))> {
        if (!is_running_)
            throw runtime_error("the threadpool is already stopped!");

        //typename std::result_of<F(Args...)>::type
        using ret_type = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<ret_type()>>(std::bind(std::forward<F>(f), forward<Args>(args)...));
        std::future<ret_type> future = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.emplace([task]{
                (*task)();
            });
        }
        //if (idle_thread_num_ < 1 && pool_.size() < THREADPOOL_MAX_NUM)
        //    addThread(1);
        task_cv_.notify_one();
        return future;
    }
private:
    std::vector<std::thread> pool_;
    std::queue<Task>         tasks_;
    std::mutex               mutex_;
    std::condition_variable  task_cv_;
    std::atomic<bool>        is_running_{true};
    std::atomic<int>         idle_thread_num_{0};
};

void func() {
    cout<<"func"<<endl;
}

void func1(int a, int b, std::string str) {
    cout<<"func1, a:"<<a<<", b:"<<b<<", str:"<<str<<endl;
}

class Test
{
public:
    int func(int a, int b) {
        cout<<"Test::func, a:"<<a<<", b:"<<b<<endl;
        return a + b;
    }

    static int s_func(int a, int b, string str) {
        cout<<"Test::s_func, a:"<<a<<", b:"<<b<<", str:"<<str<<endl;
    }

    vector<int> get_vector() {
        std::vector<int> vec_test{1, 2 ,3};
        return std::move(vec_test);
    }
};

int main()
{
    ThreadPoolSimple simple_pool{1};
    simple_pool.commit(func);
    simple_pool.commit(func);
    simple_pool.commit(func);

    ThreadPool pool{2};
    pool.commit([]{std::cout<<"lambda func"<<std::endl;});
    pool.commit(func1, 1, 2, "abc");
    std::function<void(int, int)> lambda_func = std::bind(func1, std::placeholders::_1, std::placeholders::_2, "test");
    pool.commit(lambda_func, 3, 4);

    Test test;
    auto result = pool.commit(std::bind(&Test::func, test, 5, 6)); // class common func
    int ret = result.get();
    std::cout<<"threadpool get result from Test::func, ret:"<<ret<<std::endl;

    pool.commit(Test::s_func, 7, 8, "hello"); // class static func

    std::future<vector<int>> vec_result = pool.commit(std::bind(&Test::get_vector, test));
    std::vector<int> ret1 = vec_result.get();
    std::cout<<"threadpool get result from Test::get_vector, ret:"<<ret1[0]<<std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}