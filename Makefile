all:
	g++ -Wall -std=c++11 -o test main.cpp ThreadPool.cpp TaskQueue.cpp Task.cpp

