#ifndef THREAD_POOL_TASK_QUEUE_H
#define THREAD_POOL_TASK_QUEUE_H

#include <pthread.h>
#include "Task.h"

/**
 * 任务队列的声明，基于数组实现队列结构.
 * 这里采用生产者-消费者模式，对于该队列来说，分为两种角色:
 *  - 生产者: 向队列中投放任务的线程，通常指线程池的使用者，当队列已满后则会阻塞生产者。
 *  - 消费者: 从队列中获取任务的线程，通常指线程池中的工作线程，当队列已空后则会阻塞消费者。
 */
class TaskQueue {
    Task* array; // 任务容器
    int size;    // 队列中的任务数量
    int capacity;// 队列容量
    int headIdx; // 头部索引
    int tailIdx; // 尾部索引
public:
    // 构造函数与析构函数
    explicit TaskQueue(int capacity);
    ~TaskQueue();

    // 向队列中添加一个任务
    bool addTask(Task task);
    bool addTask(Callback callback, void* arg);

    // 从队列中取出一个任务
    Task* getTask();
    // 获取当前队列中的任务数量
    int getTaskSize();

    // 当期任务队列是否已满
    bool isFull();
};


#endif
