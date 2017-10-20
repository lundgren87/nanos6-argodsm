/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
	
	Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "DataAccessRegistration.hpp"

#include "executors/threads/TaskFinalization.hpp"
#include "executors/threads/ThreadManager.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "tasks/Task.hpp"
#include "tasks/TaskImplementation.hpp"


extern "C" void *nanos_get_current_event_counter()
{
	WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
	assert(currentThread != nullptr);
	
	Task *currentTask = currentThread->getTask();
	assert(currentTask != nullptr);
	
	return currentTask;
}


extern "C" void nanos_increase_current_task_event_counter(__attribute__((unused)) void *event_counter, unsigned int increment)
{
	assert(event_counter != 0);
	if (increment == 0) return;
	
	WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
	assert(currentThread != nullptr);
	
	Task *currentTask = currentThread->getTask();
	assert(currentTask != nullptr);
	assert(event_counter == currentTask);
	
	currentTask->increaseReleaseCount(increment);
}


extern "C" void nanos_decrease_task_event_counter(void *event_counter, unsigned int decrement)
{
	assert(event_counter != 0);
	if (decrement == 0) return;
	
	Task *task = static_cast<Task *>(event_counter);
	
	// Release dependencies if the event counter becomes zero
	if (task->decreaseReleaseCount(decrement)) {
		// Release the accesses
		DataAccessRegistration::unregisterTaskDataAccesses(task, nullptr);
		
		// Try to dispose the task
		if (task->markAsReleased()) {
			TaskFinalization::disposeOrUnblockTask(task, nullptr);
		}
	}
}
