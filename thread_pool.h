#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <queue>
#include <vector>
#include <atomic>
#include <memory>
#include <future>


// 线程安全的队列
template <class T>
class SafeQueue
{
private:
    std::queue<T> task_queue;

    std::mutex task_queue_mtx;
public:

    SafeQueue() = default;

    SafeQueue(const SafeQueue<T>& other) = delete;
    SafeQueue& operator=(const SafeQueue<T>& other) = delete;
    
    SafeQueue(const SafeQueue<T>&& other) = delete;
    SafeQueue&& operator=(const SafeQueue<T>&& other) = delete;

    ~SafeQueue() = default;

    // 向队列添加元素
    void enqueue(T item)
    {
        std::unique_lock<std::mutex> lck(task_queue_mtx); // 加锁，保证任务队列写独占

        task_queue.push(item);
    }

    // 从队列取出元素
    bool dequeue(T& t)
    {
        std::unique_lock<std::mutex> lck(task_queue_mtx); // 加锁，保证只有一个线程从任务队列中取任务时

        if(task_queue.empty())
            return false;

        t = std::move(task_queue.front());

        task_queue.pop();

        return true;
   }

    bool empty()
    { 
        std::unique_lock<std::mutex> lck(task_queue_mtx);

        if(task_queue.empty())
        {
            return true;
        }

        return false;
    }

};


// 线程池, 任务类型为 std::function(void())
class ThreadPool
{
private:
    SafeQueue<std::function<void()>> m_task_queue;

    std::condition_variable m_cv;
    std::mutex m_thread_mtx;

    int m_size; // 线程池线程数量
    
    bool m_shutdown; // 线程池关闭

    std::vector<std::thread> m_threads;// 线程集合

    // 线程工作类
    class ThreadWorker
    {
    private:
        const int m_id; // 工作id
        ThreadPool *m_pool; // 所属线程池 

    public:
        ThreadWorker(ThreadPool* pool, const int id) : m_pool(pool), m_id(id)
        {}

        void operator()(){
            std::function<void()> func; // T func;
            bool dequeued;

            while (!m_pool->m_shutdown)
            {
                std::unique_lock<std::mutex> lock(m_pool->m_thread_mtx);
                if(m_pool->m_task_queue.empty())
                {
                    m_pool->m_cv.wait(lock);
                }

                dequeued = m_pool->m_task_queue.dequeue(func); // 队列中的函数都是 void (* pf)() 类型
                // 解锁执行
                lock.unlock();
                
                if(dequeued)
                    func();

            }
        }


    };
    
public:
    ThreadPool(int size) : m_size(size), m_threads(std::vector<std::thread>(size)), m_shutdown(false)
    {
        for(int i = 0; i < m_size; i++)
        {
            m_threads.at(i) = std::thread(ThreadWorker(this, i));
        }
    }

    void shutdow()
    {
        m_shutdown = true;

        m_cv.notify_all();

        for (int i = 0; i < m_threads.size(); ++i)
        {
            if(m_threads.at(i).joinable())
            {
                m_threads.at(i).join();
            }
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args &&...args) ->std::future<decltype(f(args...))>
    {
        // 创建一个函数并绑定参数准备去执行
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // func 放到packaged_task 中， 并由智能指针管理
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // 将package_task 解包为void f()
        std::function<void()> warraped_func = [task_ptr](){
            (*task_ptr)();
        };

        // 向任务队列中添加任务
        m_task_queue.enqueue(warraped_func);

        // 唤醒一个等待中的线程
        m_cv.notify_one();

        // 返回线程注册的指针
        return task_ptr->get_future();
    }
};