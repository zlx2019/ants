#pragma once

/** ====================================================
 *  任务类的声明和实现定义.
 * ==================================================== */


/** 自定义任务回调函数类型 */
using Callback = void (*)(void*);

/**
 * 任务
 *
 * @tparam T 任务函数的参数泛型
 */
template<class T>
class Task{
    Callback callback;     // 任务的回调函数
    T*  arg;               // 回调函数的参数
public:
    // 构造函数/析构函数声明
    Task<T>(Callback callback, void* arg);
    ~Task();
    // attribute getter
    Callback getCallback();
    T* getArg();
};

/**
 * 任务构造函数
 *
 * @param callback  回调函数地址
 * @param arg       函数参数
 * @tparam T        函数参数类型
 */
template<class T>
Task<T>::Task(Callback callback, void* arg) {
    this->callback = callback;
    // 将参数void* 强转为 T*
    this->arg = static_cast<T*>(arg);
}

/**
 * 任务析构函数，释放参数内存
 *
 * @tparam T   回调函数参数类型
 */
template<class T>
Task<T>::~Task() {
    // TODO 是否需要由线程池主动 回收 arg 的内存，看需求.
    // 需要则打开这段注释
    // if (arg != nullptr){
    //    delete arg;
    //    arg = nullptr;
    //}
}

template<class T>
Callback Task<T>::getCallback() {
    return callback;
}
template<class T>
T* Task<T>::getArg() {
    return arg;
}

