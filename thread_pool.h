#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <atomic>

struct TPTaskQueue {
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<int> taskCount{0};
    std::atomic<bool> shutdown{false};

    void addTask(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push(std::move(task));
            taskCount++;
        }
        cv.notify_one();
    }

    bool getTask(std::function<void()>& task) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !tasks.empty() || shutdown; });
        
        if (shutdown && tasks.empty()) {
            return false;
        }
        
        if (!tasks.empty()) {
            task = std::move(tasks.front());
            tasks.pop();
            taskCount--;
            return true;
        }
        return false;
    }

    void requestShutdown() {
        shutdown = true;
        cv.notify_all();
    }

    void waitForAll() {
        while (taskCount > 0) {
            std::this_thread::yield();
        }
    }
};

struct TPThread{
    int id;
    std::thread cur_thread;
    std::function<void()> task;
    TPTaskQueue* taskQueue;

    TPThread(int id, TPTaskQueue* queue) : id(id), taskQueue(queue) {
        cur_thread = std::thread([this]() { run(); });
    }

    void run(){
        while(true){
            if (!taskQueue->getTask(task)) {
                break; // Shutdown requested
            }
            
            if(task){ 
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "Task exception in thread " << id << ": " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown exception in thread " << id << std::endl;
                }
                task = nullptr;
            }
        }
    }

    void stop(){
        if(cur_thread.joinable()){
            cur_thread.join();
        }
    }
};

class TPThreadPool {
private:
    TPTaskQueue taskQueue;
    std::vector<TPThread> threads;
    int numThreads;
    std::atomic<bool> destroyed{false};

public:
    TPThreadPool(int n = std::thread::hardware_concurrency()) : numThreads(n) {
        if (n <= 0) {
            throw std::invalid_argument("Thread count must be positive");
        }
        
        threads.reserve(numThreads);
        for(int i = 0; i < numThreads; ++i){
            threads.emplace_back(i, &taskQueue);
        }
    }

    ~TPThreadPool(){
        shutdown();
    }

    void shutdown() {
        if (!destroyed.exchange(true)) {
            taskQueue.requestShutdown();
            for(auto& thread : threads){
                thread.stop();
            }
        }
    }

    // Enqueue a task and get a future for its result
    template<typename Func, typename... Args>
    auto enqueue(Func&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        if (destroyed.load()) {
            throw std::runtime_error("Cannot enqueue tasks on destroyed thread pool");
        }
        
        using ReturnType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(f), std::forward<Args>(args)...)
        );
        std::future<ReturnType> res = task->get_future();
        taskQueue.addTask([task]() { (*task)(); });
        return res;
    }

    void parallelFor(int start, int end, std::function<void(int)> func) {
        if (start >= end) return;
        
        std::vector<std::future<void>> futures;
        futures.reserve(end - start);
        
        for (int i = start; i < end; ++i) {
            futures.push_back(enqueue(func, i));
        }
        
        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    size_t getThreadCount() const { return numThreads; }
    size_t getPendingTaskCount() const { return taskQueue.taskCount.load(); }
};

#endif // THREAD_POOL_H