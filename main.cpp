#include <thread>
#include <chrono>
#include "ThreadPool.hpp"

/**
 * 线程池 Examples.
 */

class User{
    int no;
    const char* name;
    int age;
    char* address;
    friend void sayUser(void*);
public:
    User(int no, const char* name, int age, const char* address){
        this->no = no;
        this->name = name;
        this->age = age;
        this->address = const_cast<char*>(address);
    }
    ~User(){
//        DEBUG("~User 销毁了");
    }
};

// 自定义任务函数
void sayUser(void* arg){
    User* user = static_cast<User*>(arg);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    INFO("no: %d, name: %s, age: %d, address: %s", user->no, user->name, user->age, user->address);
    delete user;
}

int main() {
    // 创建线程池
    ThreadPool<User> pool(8, 16, 100);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 向线程池添加任务
    for (int i = 0; i < 100; i++) {
        pool.addTask(sayUser, new User(i, "abc", 100 + i, "北京"));
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 关闭线程池.
    pool.shutdown();
    return 0;
}
