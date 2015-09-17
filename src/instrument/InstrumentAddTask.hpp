#ifndef INSTRUMENT_ADD_TASK_HPP
#define INSTRUMENT_ADD_TASK_HPP


#include "api/nanos6_rt_interface.h"
#include <InstrumentTaskId.hpp>


class Task;


namespace Instrument {
	//! This function is called right after entering the runtime and must
	//! return an instrumentation-specific task identifier.
	//! The other 2 functions will also be called by the same thread sequentially.
	task_id_t enterAddTask(nanos_task_info *taskInfo, nanos_task_invocation_info *taskInvokationInfo);
	
	//! This function is called after having created the Task object and before the
	//! task can be executed.
	//! \param[in] task the Task object
	//! \param[in] taskId the task identifier returned in the call to enterAddTask
	void createdTask(Task *task, task_id_t taskId);
	
	//! This function is called right before returning to the user code. The task
	//! identifier is necessary because the actual task may have already been
	//! destroyed by the time this function is called.
	//! \param[in] taskId the task identifier returned in the call to enterAddTask
	void exitAddTask(task_id_t taskId);
}


#endif // INSTRUMENT_ADD_TASK_HPP