#ifndef THREAD_POOL_THREAD_POOL_H
#define THREAD_POOL_THREAD_POOL_H

#include <pthread.h>
#include "TaskQueue.h"

// 管理线程每次扩容或缩减工作线程的数量上限: 每轮最多扩容\缩减两个线程
#define LIMIT 2

/**
 * 线程池，属性描述:
 *  - tasks: 任务队列，用于存放生产者提交的任务
 *
 */
class ThreadPool {
    TaskQueue* tasks;   // 任务队列

    pthread_t   mgr;    // 管理线程
    pthread_t*  works;  // 工作线程组
    bool        close;  // 线程池是否被关闭

    int coreThreadNum; // 线程池的核心线程数(正常情况下，池中需要存活的线程数量)
    int maxThreadNum;  // 最大线程数
    int runThreadNum;  // 当前正在运行中的线程数量
    int totalThreadNum;// 线程池中的线程总数. totalThreadNum >= coreThreadNum ||  totalThreadNum <= maxThreadNum || totalThreadNum == (runThreadNum + waitConsumerNum)
    int deathThreadNum;// 当前需要销毁的工作线程数，0表示当前不需要销毁任何工作线程。

    pthread_cond_t producer; // 用于阻塞和唤醒生产者的条件变量
    pthread_cond_t consumer; // 用于阻塞和唤醒消费者的条件变量
    int waitProducerNum;     // 当前处于阻塞的生产者数量
    int waitConsumerNum;     // 当前处于阻塞的消费者数量(也就是处于阻塞中的工作线程数量)

    pthread_mutex_t mutex;      // 线程池全局互斥锁，主要用于同步全局状态和生产者和消费者。
    pthread_mutex_t smallMutex; // 轻量级互斥锁，用于同步修改 runThreadNum 属性，该属性的修改较为频繁

private:
    // 析构函数，释放资源
    ~ThreadPool();
    // 工作线程执行函数
    static void* worker(void* arg);
    // 管理线程执行函数
    static void* manager(void* arg);

    // 结束指定线程
    static void closeThread(pthread_t tid);
    // 将指定线程从线程组中移除
    static void removeThread(ThreadPool*, pthread_t);

public:
    // 构造函数，初始化线程池
    ThreadPool(int coreNum, int maxNum, int tasksMaxCap);

    // 关闭线程池
    void shutdown();
    // 添加任务
    void addTask(Callback callback, void* arg);

};
#endif
