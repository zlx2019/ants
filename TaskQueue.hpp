#pragma once

#include <pthread.h>
#include "Task.hpp"

/** ====================================================
 * 任务队列的声明和实现定义，基于数组实现队列结构.
 *
 * 这里采用生产者-消费者模式，对于该队列来说，分为两种角色:
 *  - 生产者: 向队列中投放任务的线程，通常指线程池的使用者，当队列已满后则会阻塞生产者。
 *  - 消费者: 从队列中获取任务的线程，通常指线程池中的工作线程，当队列已空后则会阻塞消费者。
 * ==================================================== */

/**
 * 任务队列
 *
 * @tparam T 任务的回调函数参数泛型
 */
template<typename T>
class TaskQueue {
    Task<T>** array; // 任务指针列表，存储提交的任务指针
    int size;        // 队列中的任务数量
    int capacity;    // 队列容量
    int headIdx;     // 头部索引
    int tailIdx;     // 尾部索引
public:
    // 构造函数与析构函数
    TaskQueue(int capacity);
    ~TaskQueue();

    // 向队列中添加任务
    bool addTask(Task<T>* task);
    void addTask(Callback callback, void* arg);

    // 从队列中取出一个任务
    Task<T>* getTask();
    // 获取当前队列中的任务数量
    int getTaskSize();
    // 当期任务队列是否已满
    bool isFull();
};


/**
 * 任务队列构造函数，实例化任务队列
 *
 * @tparam T 任务函数参数类型
 * @param capacity 队列的最大容量
 */
template<typename T>
TaskQueue<T>::TaskQueue(int capacity) {
    if(capacity <= 0){
        throw "TaskQueue capacity cannot be less than zero.";
    }
    this->size = 0;
    this->capacity = capacity;
    this->headIdx = 0;
    this->tailIdx = 0;
    // 为任务数组分配内存
    this->array = new Task<T>*[capacity]();
}

/**
 * 析构函数，释放队列内存
 */
template<typename T>
TaskQueue<T>::~TaskQueue() {
    delete[] array;
}

/**
 * 向队列尾部添加一个任务
 *
 * @param callback  回调函数指针
 * @param arg       回调函数参数指针
 */
template<typename T>
void TaskQueue<T>::addTask(Callback callback, void *arg) {
    // :note 基于堆内存创建一个Task
    return addTask(new Task<T>(callback, (T*)arg));
}

/**
 * 向队列尾部添加一个任务
 *
 * @param task 任务
 */
template<typename T>
bool TaskQueue<T>::addTask(Task<T>* task) {
    // 添加任务，将任务追加到到队列尾部
    array[tailIdx] = task;
    size++;
    // 后移队列尾部索引，如果超过队列容量后则重置到数组头部，形成一种环形结构.
    tailIdx = (tailIdx + 1) % capacity;
    return true;
}

/**
 * @return 从队列头部获取一个任务.
 */
template<typename T>
Task<T>* TaskQueue<T>::getTask() {
    // 获取队列头部的任务指针
    Task<T>* task = array[headIdx];
    size--;
    // 后移头部节点，超过容量后重置回数组首个元素
    headIdx = (headIdx + 1) % capacity;
    return task;
}


/**
 * 获取当前任务队列中的任务数量
 */
template<typename T>
int TaskQueue<T>::getTaskSize() {
    return size;
}

/**
 * 当前任务队列是否已满
 */
template<typename T>
bool TaskQueue<T>::isFull() {
    return size >= capacity;
}
