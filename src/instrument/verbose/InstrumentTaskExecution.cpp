/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2018 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "InstrumentTaskExecution.hpp"
#include "InstrumentVerbose.hpp"
#include "executors/threads/CPU.hpp"

#include <InstrumentInstrumentationContext.hpp>


using namespace Instrument::Verbose;


namespace Instrument {
	void startTask(task_id_t taskId, InstrumentationContext const &context) {
		if (!_verboseTaskExecution) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry(context);
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " --> Task " << taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void endTask(task_id_t taskId, InstrumentationContext const &context) {
		if (!_verboseTaskExecution) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry(context);
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-- Task " << taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void destroyTask(task_id_t taskId, InstrumentationContext const &context) {
		if (!_verboseTaskExecution) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry(context);
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> DestroyTask " << taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void startTaskforCollaborator(__attribute__((unused)) task_id_t taskforId, __attribute__((unused)) task_id_t collaboratorId, __attribute__((unused))  bool first, __attribute__((unused)) InstrumentationContext const &context) {
		// Verbose instrumentation does not instrument task fors
	}
	
	
	void endTaskforCollaborator(__attribute__((unused)) task_id_t taskforId, __attribute__((unused)) task_id_t collaboratorId, __attribute__((unused)) bool last, __attribute__((unused)) InstrumentationContext const &context) {
		// Verbose instrumentation does not instrument
	}
	
}
