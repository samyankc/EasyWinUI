#include <algorithm>
#include <atomic>
#include <iostream>
#include <queue>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <future>
#include <mutex>
#include <cmath>

template<std::size_t PoolSize = 4, typename TaskType = std::function<void()>>
struct ThreadPool
{
    constexpr static auto ThreadCount = PoolSize;
    using TaskQueueType = std::queue<TaskType>;

    struct Worker
    {
        TaskType Task;
        //std::thread       Thread;
        std::atomic<bool> Idle;
        std::atomic<bool> Spinning;

        Worker() : Idle{ true }, Spinning{ true }
        {
            //Thread =
            std::thread( [this] {
                while( Spinning )
                {
                    Idle.wait( true, std::memory_order_acquire );
                    if( Task ) Task();
                    Idle.store( true, std::memory_order_release );
                }
            } ).detach();
        }

        void Activate()
        {
            Idle.store( false, std::memory_order_release );
            Idle.notify_one();
        }

        bool Available()
        {
            if( Idle.load( std::memory_order_acquire ) ) return true;
            Idle.notify_one();
            return false;
        }

        ~Worker()
        {
            Spinning.store( false );
            Task = nullptr;
            if( Idle ) Activate();
            // Thread.join();
        }
    };

    inline static auto Workers = std::vector<Worker>( ThreadCount );

    inline static auto TaskQueue = TaskQueueType{};
    inline static auto TaskQueueFilled = std::atomic<bool>{};
    inline static auto TaskQueueMutex = std::mutex{};
    inline static auto TaskDistributor =
        ( std::thread( [] {
              while( true )
              {
                  TaskQueueFilled.wait( false, std::memory_order_acquire );

                  if( auto IdleWorker = std::ranges::find_if( Workers, &Worker::Available );
                      IdleWorker != Workers.end() )
                  {
                      IdleWorker->Task = std::move( TaskQueue.front() );
                      IdleWorker->Activate();
                      {
                          std::scoped_lock Lock( TaskQueueMutex );
                          TaskQueue.pop();
                          if( TaskQueue.empty() ) TaskQueueFilled.store( false, std::memory_order_release );
                      }
                  }
              }
          } ).detach(),
          0 );

    static void WaitComplete()
    {
        while( ! TaskQueue.empty() )
        {
            TaskQueueFilled.notify_one();
            std::this_thread::yield();
        }
        while( ! std::ranges::all_of( Workers, &Worker::Available ) ) std::this_thread::yield();
    }

    static auto async( std::launch, TaskType&& NewTask ) { return AddTask( std::move( NewTask ) ); }

    static void AddTask( TaskType&& NewTask )
    {
        std::scoped_lock Lock( TaskQueueMutex );

        TaskQueue.push( std::move( NewTask ) );
        TaskQueueFilled.store( true, std::memory_order_release );
        TaskQueueFilled.notify_one();
    }

    static void Execute( TaskQueueType&& IncomingTaskQueue )
    {
        while( ! IncomingTaskQueue.empty() )
        {
            async( std::move( IncomingTaskQueue.front() ) );
            IncomingTaskQueue.pop();
        }
    }
};