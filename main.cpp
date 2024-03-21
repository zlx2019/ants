#include <thread>
#include <chrono>
#include "ThreadPool.h"

void say(void* arg){
    int no = *(int*)arg;
    printf("say hello, no: %d threadId: %lu \n",no, std::this_thread::get_id());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {
    ThreadPool* pool = new ThreadPool(8,16,100);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (int i = 0; i < 100; i++) {
        int* num = new int(i);
        pool->addTask(say, num);
    }

    std::this_thread::sleep_for(std::chrono::seconds(15));
    pool->shutdown();
    delete pool;
    return 0;
}
