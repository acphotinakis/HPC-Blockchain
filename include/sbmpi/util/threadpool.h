#ifndef SBMPI_THREADPOOL_H
#define SBMPI_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/**
 * @file threadpool.h
 * @brief Defines a simple thread pool for concurrent task execution.
 *
 * This file provides the interface for the ThreadPool class, implemented in
 * `src/util/threadpool.cpp`. The thread pool is a utility for managing a fixed
 * number of worker threads to execute tasks from a queue, which can help
 * improve performance in concurrent sections of the simulation.
 */

class ThreadPool
{
 public:
  /**
   * @brief Constructs a ThreadPool with a specific number of threads.
   *
   * @param threads The number of worker threads to create.
   */
  ThreadPool(size_t threads);

  /**
   * @brief Destructor for the ThreadPool.
   *
   * Joins all worker threads.
   */
  ~ThreadPool();

  /**
   * @brief Enqueues a new task to be executed by the thread pool.
   *
   * @param f The function to execute.
   * @param args The arguments to the function.
   */
  template <class F, class... Args>
  void enqueue(F&& f, Args&&... args);

 private:
  // The worker threads.
  std::vector<std::thread> workers;
  // The task queue.
  std::queue<std::function<void()>> tasks;

  // Synchronization primitives.
  std::mutex              queue_mutex;
  std::condition_variable condition;
  bool                    stop;
};

#endif  // SBMPI_THREADPOOL_H