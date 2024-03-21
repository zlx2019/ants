
#include "TaskQueue.h"

/**
 * 任务队列的实现定义.
 */

/**
 * 构造函数，创建一个任务队列
 * @param capacity 队列容量
 */
TaskQueue::TaskQueue(int capacity) {
    if(capacity <= 0){
        throw "TaskQueue capacity cannot be less than zero.";
    }
    this->size = 0;
    this->capacity = capacity;
    this->headIdx = 0;
    this->tailIdx = 0;
    // 为任务数组分配内存
    this->array = new Task[capacity]();
}

/**
 * 析构函数，释放队列的资源
 */
TaskQueue::~TaskQueue() {
    if (array != nullptr){
        delete[] array;
        array = nullptr;
    }
}


bool TaskQueue::addTask(Callback callback, void *arg) {
    return addTask(Task(callback, arg));
}

/**
 * 向队列尾部添加一个任务
 *
 * @param task 任务
 */
bool TaskQueue::addTask(Task task) {
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
Task* TaskQueue::getTask() {
    Task* task = new Task(array[headIdx].getCallback(), array[headIdx].getArg());
    size--;
    // 后移头部节点，超过容量后重置回数组首个元素
    headIdx = (headIdx + 1) % capacity;
    return task;
}


/**
 * 获取当前任务队列中的任务数量
 */
int TaskQueue::getTaskSize() {
    return size;
}

/**
 * 当前任务队列是否已满
 */
bool TaskQueue::isFull() {
    return size >= capacity;
}