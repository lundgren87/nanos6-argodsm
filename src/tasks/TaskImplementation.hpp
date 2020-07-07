/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef TASK_IMPLEMENTATION_HPP
#define TASK_IMPLEMENTATION_HPP

#include "StreamExecutor.hpp"
#include "Task.hpp"
#include "hardware-counters/HardwareCounters.hpp"

#include <DataAccessRegistration.hpp>
#include <InstrumentTaskId.hpp>
#include <TaskDataAccesses.hpp>

#include <cstring>

inline Task::Task(
	void *argsBlock,
	size_t argsBlockSize,
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *taskInvokationInfo,
	Task *parent,
	Instrument::task_id_t instrumentationTaskId,
	size_t flags,
	const TaskDataAccessesInfo &taskAccessInfo,
	const TaskHardwareCountersInfo &taskCounters
) :
	_argsBlock(argsBlock),
	_argsBlockSize(argsBlockSize),
	_taskInfo(taskInfo),
	_taskInvokationInfo(taskInvokationInfo),
	_countdownToBeWokenUp(1),
	_removalCount(1),
	_parent(parent),
	_priority(0),
	_deadline(0),
	_schedulingHint(NO_HINT),
	_thread(nullptr),
	_dataAccesses(taskAccessInfo),
	_flags(flags),
	_predecessorCount(0),
	_instrumentationTaskId(instrumentationTaskId),
	_computePlace(nullptr),
	_memoryPlace(nullptr),
	_countdownToRelease(1),
	_workflow(nullptr),
	_executionStep(nullptr),
	_taskStatistics(),
	_taskPredictions(),
	_hwCounters(taskCounters),
	_clusterContext(nullptr),
	_parentSpawnCallback(nullptr),
	_nestingLevel(0)
{
	if (parent != nullptr) {
		parent->addChild(this);
		_nestingLevel = parent->getNestingLevel() + 1;
	}
}

inline Task::~Task()
{
	if (_clusterContext != nullptr) {
		delete _clusterContext;
	}

	// Destroy hardware counters
	_hwCounters.shutdown();
}

inline void Task::reinitialize(
	void *argsBlock,
	size_t argsBlockSize,
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *taskInvokationInfo,
	Task *parent,
	Instrument::task_id_t instrumentationTaskId,
	size_t flags
) {
	_argsBlock = argsBlock;
	_argsBlockSize = argsBlockSize;
	_taskInfo = taskInfo;
	_taskInvokationInfo = taskInvokationInfo;
	_countdownToBeWokenUp = 1;
	_removalCount = 1;
	_parent = parent;
	_priority = 0;
	_deadline = 0;
	_schedulingHint = NO_HINT;
	_thread = nullptr;
	_flags = flags;
	_predecessorCount = 0;
	_instrumentationTaskId = instrumentationTaskId;
	_computePlace = nullptr;
	_memoryPlace = nullptr;
	_countdownToRelease = 1;
	_workflow = nullptr;
	_executionStep = nullptr;
	_clusterContext = nullptr;
	_parentSpawnCallback = nullptr;
	_nestingLevel = 0;

	if (parent != nullptr) {
		parent->addChild(this);
		_nestingLevel = parent->getNestingLevel() + 1;
	}

	// Re-use hardware counters
	HardwareCounters::taskReinitialized(this);
}

inline bool Task::markAsFinished(ComputePlace *computePlace)
{
	_thread = nullptr;
	_computePlace = nullptr;

	// If the task has a wait clause, the release of dependencies must be
	// delayed (at least) until the task finishes its execution and all
	// its children complete and become disposable
	if (mustDelayRelease()) {
		CPUDependencyData hpDependencyData;

		//! We need to pass 'nullptr' here as a ComputePlace to notify
		//! the DataAccessRegistration system that it is creating
		//! taskwait fragments for a 'wait' task.
		DataAccessRegistration::handleEnterTaskwait(this, nullptr, hpDependencyData);

		if (!markAsBlocked()) {
			return false;
		}

		// All its children are completed, so the delayed release of
		// dependencies has successfully completed
		completeDelayedRelease();
		DataAccessRegistration::handleExitTaskwait(this, computePlace, hpDependencyData);
		markAsUnblocked();
	}

	// Return whether all external events have been also fulfilled, so
	// the dependencies can be released
	return decreaseReleaseCount();
}

// Return if the task can release its dependencies
inline bool Task::markAllChildrenAsFinished(ComputePlace *computePlace)
{
	assert(_thread == nullptr);

	CPUDependencyData hpDependencyData;

	// Complete the delayed release of dependencies
	completeDelayedRelease();
	DataAccessRegistration::handleExitTaskwait(this, computePlace, hpDependencyData);
	markAsUnblocked();

	// Return whether all external events have been also fulfilled, so
	// the dependencies can be released
	return decreaseReleaseCount();
}

#endif // TASK_IMPLEMENTATION_HPP
