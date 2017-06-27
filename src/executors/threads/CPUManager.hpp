#ifndef CPU_MANAGER_HPP
#define CPU_MANAGER_HPP


#include <mutex>
#include <atomic>

#include <boost/dynamic_bitset.hpp>

#include "hardware/places/ComputePlace.hpp"
#include "lowlevel/SpinLock.hpp"
#include "lowlevel/FatalErrorHandler.hpp"

#include "CPU.hpp"


class CPUManager {
private:
	//! \brief per-CPU data indexed by system CPU identifier
	static std::vector<CPU *> _cpus;

	//! \brief number of available CPUs
	static size_t _totalCPUs;
	
	//! \brief indicates if the thread manager has finished initializing the CPUs
	static std::atomic<bool> _finishedCPUInitialization;
	
	//! \brief threads blocked due to idleness
	static boost::dynamic_bitset<> _idleCPUs;

	//! \brief NUMA node CPU mask
	static std::vector<boost::dynamic_bitset<>> _NUMANodeMask;

	static SpinLock _idleCPUsLock;
	
public:
	static void preinitialize();
	
	static void initialize();
	
	//! \brief get the CPU object assigned to a given numerical system CPU identifier
	static inline CPU *getCPU(size_t systemCPUId);
	
	//! \brief get the maximum number of CPUs that will be used
	static inline long getTotalCPUs();
	
	//! \brief check if initialization has finished
	static inline bool hasFinishedInitialization();
	
	//! \brief get a reference to the list of CPUs
	static inline std::vector<CPU *> const &getCPUListReference();

	//! \brief mark a CPU as idle
	static inline void cpuBecomesIdle(CPU *cpu);

	//! \brief get an idle CPU
	static inline CPU *getIdleCPU();

	//! \brief get an idle CPU from a specific NUMA node
	static inline CPU *getIdleNUMANodeCPU(size_t NUMANodeId);
	
	//! \brief mark a CPU as not being idle (if possible)
	static inline bool unidleCPU(CPU *cpu);
};


inline CPU *CPUManager::getCPU(size_t systemCPUId)
{
	// _cpus is sorted by virtual ID. This search should be changed in the future
	for (size_t i = 0; i < _cpus.size(); ++i) {
		if (_cpus[i] != nullptr && _cpus[i]->_systemCPUId == systemCPUId) {
			return _cpus[i];
		}
	}

	assert(false);
	return nullptr;
}

inline long CPUManager::getTotalCPUs()
{
	return _totalCPUs;
}

inline bool CPUManager::hasFinishedInitialization()
{
	return _finishedCPUInitialization;
}


inline std::vector<CPU *> const &CPUManager::getCPUListReference()
{
	return _cpus;
}


inline void CPUManager::cpuBecomesIdle(CPU *cpu)
{
	std::lock_guard<SpinLock> guard(_idleCPUsLock);
	_idleCPUs[cpu->_virtualCPUId] = true;
}


inline CPU *CPUManager::getIdleCPU()
{
	std::lock_guard<SpinLock> guard(_idleCPUsLock);
	boost::dynamic_bitset<>::size_type idleCPU = _idleCPUs.find_first();
	if (idleCPU != boost::dynamic_bitset<>::npos) {
		_idleCPUs[idleCPU] = false;
		return _cpus[idleCPU];
	} else {
		return nullptr;
	}
}


inline CPU *CPUManager::getIdleNUMANodeCPU(size_t NUMANodeId)
{
	std::lock_guard<SpinLock> guard(_idleCPUsLock);
	boost::dynamic_bitset<> tmpIdleCPUs = _idleCPUs & _NUMANodeMask[NUMANodeId];
	boost::dynamic_bitset<>::size_type idleCPU = tmpIdleCPUs.find_first();
	if (idleCPU != boost::dynamic_bitset<>::npos) {
		_idleCPUs[idleCPU] = false;
		return _cpus[idleCPU];
	} else {
		return nullptr;
	}
}


inline bool CPUManager::unidleCPU(CPU *cpu)
{
	assert(cpu != nullptr);
	
	std::lock_guard<SpinLock> guard(_idleCPUsLock);
	if (_idleCPUs[cpu->_virtualCPUId]) {
		_idleCPUs[cpu->_virtualCPUId] = false;
		return true;
	} else {
		return false;
	}
}

#endif // CPU_MANAGER_HPP
