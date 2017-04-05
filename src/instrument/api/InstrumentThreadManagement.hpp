#ifndef INSTRUMENT_THREAD_MANAGEMENT_HPP
#define INSTRUMENT_THREAD_MANAGEMENT_HPP


#include <InstrumentInstrumentationContext.hpp>


namespace Instrument {
	//! This function is called when the runtime creates a new thread and
	//! must return an instrumentation-specific thread identifier that will
	//! be used to identify it throughout the rest of the instrumentation API.
	thread_id_t createdThread();
	
	void threadWillSuspend(thread_id_t threadId, hardware_place_id_t cpu);
	void threadHasResumed(thread_id_t threadId, hardware_place_id_t cpu);
}


#endif // INSTRUMENT_THREAD_MANAGEMENT_HPP