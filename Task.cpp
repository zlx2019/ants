#include "Task.h"
#include <iostream>

/**
 * Task 任务类 实现.
 */


// Task 构造函数实现定义
Task::Task( Callback callback, void* arg): callback(callback), arg(arg) {}
Task::Task(): Task(nullptr, nullptr) {}

Task::~Task() {
}


Callback Task::getCallback() {
    return callback;
}
void *Task::getArg() {
    return arg;
}