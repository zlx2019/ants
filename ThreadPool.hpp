#pragma once
#include <pthread.h>
#include <chrono>
#include <thread>
#include "TaskQueue.hpp"
#include "Log.hpp"

/** ====================================================
 * 线程池的声明和实现定义.
 *
 * ==================================================== */

// 管理线程每次扩容或缩减工作线程的数量上限: 每轮最多扩容或缩减两个线程
#define LIMIT 2

/**
 * 线程池
 *  - tasks: 任务队列，用于存放生产者提交的任务
 */
template<class T>
class ThreadPool {
    TaskQueue<T>* tasks;    // 任务队列

    pthread_t   mgr;        // 管理线程
    pthread_t*  works;      // 工作线程组
    bool        close;      // 线程池是否被关闭

    int coreThreadNum;      // 线程池的核心线程数(正常情况下，池中需要存活的线程数量)
    int maxThreadNum;       // 最大线程数
    int runThreadNum;       // 当前正在运行中的线程数量
    int totalThreadNum;     // 线程池中的线程总数. totalThreadNum >= coreThreadNum ||  totalThreadNum <= maxThreadNum || totalThreadNum == (runThreadNum + waitConsumerNum)
    int deathThreadNum;     // 当前需要销毁的工作线程数，0表示当前不需要销毁任何工作线程。

    pthread_cond_t producer; // 用于阻塞和唤醒生产者的条件变量
    pthread_cond_t consumer; // 用于阻塞和唤醒消费者的条件变量
    int waitProducerNum;     // 当前处于阻塞的生产者数量
    int waitConsumerNum;     // 当前处于阻塞的消费者数量(也就是处于阻塞中的工作线程数量)

    pthread_mutex_t mutex;      // 线程池全局互斥锁，主要用于同步全局状态和生产者和消费者。
    pthread_mutex_t smallMutex; // 轻量级互斥锁，用于同步修改 runThreadNum 属性，该属性的修改较为频繁

private:

    // 工作线程执行函数
    static void* worker(void* arg);
    // 管理线程执行函数
    static void* manager(void* arg);

    // 结束指定线程
    static void closeThread();
    // 将指定线程从线程组中移除
    static void removeThread(ThreadPool*, pthread_t);

public:
    // 构造函数，初始化线程池
    ThreadPool(int coreNum, int maxNum, int tasksMaxCap);
    // 析构函数，释放资源
    ~ThreadPool();
    // 关闭线程池
    void shutdown();
    // 添加任务
    void addTask(Callback callback, void* arg);

};


/**
 * 创建并且初始化一个线程池
 * @param coreNum       核心线程数
 * @param maxNum        最大线程数
 * @param tasksMaxCap   任务队列容量
 */
template<class T>
ThreadPool<T>::ThreadPool(int coreNum, int maxNum, int tasksMaxCap) {
    // 初始化线程池基本属性
    coreThreadNum = coreNum;
    maxThreadNum = maxNum;
    runThreadNum = 0;
    deathThreadNum = 0;
    totalThreadNum = coreThreadNum;
    close = false;

    // 初始化任务队列
    tasks = new TaskQueue<T>(tasksMaxCap);
    // 初始化工作线程组
    works = new pthread_t[coreThreadNum]{nullptr};

    // 初始化同步器
    pthread_mutex_init(&mutex,nullptr);
    pthread_mutex_init(&smallMutex, nullptr);
    pthread_cond_init(&producer, nullptr);
    pthread_cond_init(&consumer, nullptr);
    waitProducerNum = 0;
    waitConsumerNum = 0;

    // 创建并且运行工作线程组
    for (int i = 0; i < coreThreadNum; i++)
        pthread_create(&works[i], nullptr, worker, this);
    // 创建并且运行管理线程
    pthread_create(&mgr, nullptr, manager, this);
}

/**
 * 线程池析构函数，释放内存
 */
template<class T>
ThreadPool<T>::~ThreadPool() {
    delete tasks;
    delete[] works;
}

/**
 * 关闭线程池
 */
template<class T>
void ThreadPool<T>::shutdown() {
    // 标识为已关闭状态
    pthread_mutex_lock(&mutex);
    close = true;
    pthread_mutex_unlock(&mutex);
    // 关闭管理线程
    pthread_join(mgr, nullptr);
    // 唤醒所有处于阻塞的生产者(如果有)
    for (int i = 0; i < waitProducerNum; i++) {
        pthread_cond_broadcast(&producer);
    }
    // 唤醒所有处于阻塞的消费者(工作线程)
    for (int i = 0; i < waitConsumerNum; i++) {
        pthread_cond_broadcast(&consumer);
    }
    // 等待目前还在执行任务中的工作线程
    for (int i = 0; i < maxThreadNum; i++) {
        if (works[i] != nullptr){
            pthread_join(works[i], nullptr);
        }
    }
    // 销毁同步器
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&smallMutex);
    pthread_cond_destroy(&producer);
    pthread_cond_destroy(&consumer);
}

/**
 * 线程池管理线程的执行函数.
 * 管理线程的职责:
 *      动态调整工作线程的数量，任务过多则扩容，过少则恢复核心线程数量。
 * 调整规则如下:
 *    - 扩容条件: 所有线程都在执行任务 && (当前线程总数 < 当前队列中的任务数量) && (当前线程总数 < 最大线程数)
 *    - 缩减条件: (当前线程总数 > 核心线程数量) && (当前运行中的线程数量 < 当前线程总数 / 2)
 *
 *
 * @param arg   线程池实例
 */
template<class T>
void* ThreadPool<T>::manager(void* arg) {
    ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);
    int maxThreads = pool->maxThreadNum;
    int coreThreads = pool->coreThreadNum;

    // 线程池不关闭，就不断的运行维护.
    while (!pool->close){
        // 每隔1秒执行一轮
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 加锁，获取当前线程池的一些可变状态
        pthread_mutex_lock(&pool->mutex);
        int taskSize = pool->tasks->getTaskSize();  // 当前队列中的任务数量
        int totalThreads = pool->totalThreadNum;    // 当前所有线程数量
        int runThreads = pool->runThreadNum;        // 当前正在运行中的线程数量
        pthread_mutex_unlock(&pool->mutex);

        // 是否需要扩容工作线程
        //
        if (runThreads == totalThreads && totalThreads < taskSize && totalThreads < maxThreads){
            // note 扩容工作线程
            // 每次扩容最多不超过 LIMIT 个线程
            int incrLimit = maxThreads - totalThreads > LIMIT ? LIMIT : maxThreads - totalThreads;
            int incr = 0;
            for (int i = 0; i < maxThreads && incr < incrLimit; i++) {
                // 在线程组中找到一个没有占用的空位
                if (pool->works[i] == 0){
                    pthread_create(&pool->works[i], nullptr, worker, pool);
                    incr++;
                    pthread_mutex_lock(&pool->mutex);
                    pool->totalThreadNum++;
                    pthread_mutex_unlock(&pool->mutex);
                }
            }
        }

        // 是否需要缩减工作线程
        // (当前线程总数 > 核心线程数量) && (当前运行中的线程数量 < 当前线程总数 / 2)
        if (totalThreads > coreThreads && runThreads * 2 < totalThreads){
            // note 缩减工作线程
            // 每次缩减最多不超过 LIMIT 个线程
            int decrLimit = totalThreads - coreThreads > LIMIT ? LIMIT : totalThreads - coreThreads;
            pthread_mutex_lock(&pool->mutex);
            pool->deathThreadNum = decrLimit;
            pthread_mutex_unlock(&pool->mutex);
            // 唤醒 decrLimit 个处于阻塞的消费者(工作线程)
            for (int i = 0; i < decrLimit; i++) {
                pthread_cond_signal(&pool->consumer);
            }
        }
    }
    return nullptr;
}

/**
 * 工作线程的执行函数
 * @param arg   线程池实例
 */
template<class T>
void* ThreadPool<T>::worker(void* arg) {
    pthread_t tid = pthread_self();
    INFO("WorkThread-[%lu] running~", tid);
    // 将参数转换为线程池实例
    ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);

    // 周而复始的处理任务...
    while (true){
        // 对线程池加锁
        pthread_mutex_lock(&pool->mutex);
        while (pool->tasks->getTaskSize() == 0 && !pool->close){
            // 任务队列为空，阻塞当前工作线程，等待生产者唤醒
            pool->waitConsumerNum++;
            pthread_cond_wait(&pool->consumer, &pool->mutex);
            // :mark Blocking...
            // 被唤醒...
            pool->waitConsumerNum--;
            // 判断是否由于工作线程过于空闲,而需要销毁该工作线程
            if (pool->deathThreadNum > 0){
                pool->deathThreadNum--;
                pool->totalThreadNum--;
                pthread_mutex_unlock(&pool->mutex);
                removeThread(pool,tid); // 从线程组中移除，并且结束线程
            }
        }
        // 由于线程池被关闭，导致被唤醒
        if (pool->close){
            pool->totalThreadNum--;
            pthread_mutex_unlock(&pool->mutex);
            closeThread(); // 结束线程
        }
        // 获取任务
        Task<T>* task = pool->tasks->getTask();
        // mark 是否需要唤醒生产者
        if (pool->waitProducerNum > 0){
            pthread_cond_signal(&pool->producer);
        }
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&pool->smallMutex);
        pool->runThreadNum++;
        pthread_mutex_unlock(&pool->smallMutex);

        // :note 执行任务
        task->getCallback()(task->getArg());

        pthread_mutex_lock(&pool->smallMutex);
        pool->runThreadNum--;
        pthread_mutex_unlock(&pool->smallMutex);
        delete task; // 释放任务内存
    }
}


/**
 * 添加任务到线程池中
 * @param callback  任务的回调函数
 * @param arg       函数的参数
 */
template<class T>
void ThreadPool<T>::addTask(Callback callback, void *arg) {
    pthread_mutex_lock(&mutex);
    while (!close && tasks->isFull()){
        // mark 任务队列已满，阻塞生产者
        waitProducerNum++;
        pthread_cond_wait(&producer, &mutex);
        waitProducerNum--;
    }
    if (close){
        pthread_mutex_unlock(&mutex);
        return;
    }
    // :mark 封装任务，并且添加到队列
    tasks->addTask(new Task<T>(callback,arg));
    // :note 唤醒阻塞的消费者
    if (waitConsumerNum > 0){
        pthread_cond_signal(&consumer);
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * 将指定线程从线程组中移除
 */
template<class T>
void ThreadPool<T>::removeThread(ThreadPool<T>* pool, pthread_t tid) {
    // 在线程组中找到当前线程，将其替换为0，表示空位
    for (int i = 0; i < pool->maxThreadNum; i++) {
        if (pool->works[i] == tid){
            pool->works[i] = nullptr;
            break;
        }
    }
    closeThread();
}

/**
 * 结束指定线程
 *
 * @param tid 要结束的线程id
 */
template<class T>
void ThreadPool<T>::closeThread() {
    INFO("WorkThread-[%lu] close~",pthread_self());
    pthread_exit(nullptr);
}

