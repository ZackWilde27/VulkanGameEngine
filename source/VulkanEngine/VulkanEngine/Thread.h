#pragma once
#include "engineTypes.h"
#include <thread>

// Takes the user data given to the thread on creation and returns whether or not the thread should exit
typedef bool (*zThreadFunc)(void*);

void zThreadTick(Thread* thread);

class Thread
{
	std::thread* thread;

public:
	VkBool32 shouldClose;
	void* udata;
	zThreadFunc function;

	Thread(zThreadFunc func, void* data);
	~Thread();
};