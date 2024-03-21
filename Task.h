
#ifndef THREAD_POOL_TASK_H
#define THREAD_POOL_TASK_H

// 自定义任务函数的类型
using Callback = void (*)(void*);

/**
 * 任务类，线程池任务队列中的提交元素.
 */
class Task{
    Callback callback;       // 任务的回调函数
    void *arg;               // 回调函数的参数
public:
    // 构造函数声明
    Task(Callback callback, void* arg);
    Task();

    // 获取任务回调函数
    Callback getCallback();
    // 获取回调函数参数
    void* getArg();
};


#endif
