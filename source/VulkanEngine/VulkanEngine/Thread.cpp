#include "Thread.h"

void zThreadTick(Thread* thread)
{
	while (!thread->shouldClose)
		if (thread->function(thread->udata)) break;
}

Thread::Thread(zThreadFunc func, void* data)
{
	shouldClose = VK_FALSE;
	udata = data;
	function = func;

	thread = new std::thread(zThreadTick, this);
}

Thread::~Thread()
{
	shouldClose = VK_TRUE;
	thread->join();
	delete thread;
}