#ifndef INSTRUMENT_EXTRAE_DEPENDENCIES_BY_ACCESS_HPP
#define INSTRUMENT_EXTRAE_DEPENDENCIES_BY_ACCESS_HPP


#include <InstrumentTaskId.hpp>

#include "../InstrumentDependenciesByAccess.hpp"
#include "dependencies/DataAccessType.hpp"


namespace Instrument {
	inline void registerTaskAccess(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) DataAccessType accessType,
		__attribute__((unused)) bool weak,
		__attribute__((unused)) void *start,
		__attribute__((unused)) size_t length)
	{
	}
}


#endif // INSTRUMENT_EXTRAE_DEPENDENCIES_BY_ACCESS_HPP