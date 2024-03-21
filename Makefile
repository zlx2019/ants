all:
	g++ -Wall -std=c++11 -lpthread -o test main.cpp ThreadPool.cpp TaskQueue.cpp Task.cpp

