#include "ThreadPool.h"
#include "Log.h"
#include <chrono>
#include <thread>


/**
 * 创建并且初始化一个线程池
 * @param coreNum       核心线程数
 * @param maxNum        最大线程数
 * @param tasksMaxCap   任务队列容量
 */
ThreadPool::ThreadPool(int coreNum, int maxNum, int tasksMaxCap) {
    // 初始化线程池基本属性
    coreThreadNum = coreNum;
    maxThreadNum = maxNum;
    runThreadNum = 0;
    deathThreadNum = 0;
    totalThreadNum = coreThreadNum;
    close = false;

    // 初始化任务队列
    tasks = new TaskQueue(tasksMaxCap);
    // 初始化工作线程组
    works = new pthread_t[coreThreadNum]{0x00};

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
ThreadPool::~ThreadPool() {

}

/**
 * 关闭线程池
 */
void ThreadPool::shutdown() {
    if (close){
        return;
    }
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
        if (works[i] != 0){
            pthread_join(works[i], nullptr);
        }
    }
    delete[] works;
    delete tasks;
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
void* ThreadPool::manager(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
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
void* ThreadPool::worker(void* arg) {
    pthread_t tid = pthread_self();
    INFO("WorkThread-[%lu] running~", tid);
    // 将参数转换为线程池实例
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

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
        Task* task = pool->tasks->getTask();
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
void ThreadPool::addTask(Callback callback, void *arg) {
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
    tasks->addTask(Task(callback,arg));
    // :note 唤醒阻塞的消费者
    if (waitConsumerNum > 0){
        pthread_cond_signal(&consumer);
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * 将指定线程从线程组中移除
 */
void ThreadPool::removeThread(ThreadPool* pool, pthread_t tid) {
    // 在线程组中找到当前线程，将其替换为0，表示空位
    for (int i = 0; i < pool->maxThreadNum; i++) {
        if (pool->works[i] == tid){
            pool->works[i] = 0x00;
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
void ThreadPool::closeThread() {
    INFO("WorkThread-[%lu] close~",pthread_self());
    pthread_exit(nullptr);
}