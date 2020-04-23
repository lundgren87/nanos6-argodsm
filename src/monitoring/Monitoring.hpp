/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef MONITORING_HPP
#define MONITORING_HPP

#include "CPUMonitor.hpp"
#include "TaskMonitor.hpp"
#include "WorkloadPredictor.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "support/JsonFile.hpp"
#include "support/config/ConfigVariable.hpp"
#include "tasks/Task.hpp"


namespace BoostAcc = boost::accumulators;
namespace BoostAccTag = boost::accumulators::tag;

class Monitoring {

private:

	//! Whether monitoring has to be performed or not
	static ConfigVariable<bool> _enabled;

	//! Whether verbose mode is enabled
	static ConfigVariable<bool> _verbose;

	//! Whether the wisdom mechanism is enabled
	static ConfigVariable<bool> _wisdomEnabled;

	//! The file where output must be saved when verbose mode is enabled
	static ConfigVariable<std::string> _outputFile;

	//! A Json file for monitoring data
	static JsonFile *_wisdom;

	//    MONITORS    //

	//! A monitor that handles CPU statistics
	static CPUMonitor *_cpuMonitor;

	//! A monitor that handles task statistics
	static TaskMonitor *_taskMonitor;

	//! A monitor that aggregates task statistics into runtime workload stats
	static WorkloadMonitor *_workloadMonitor;

	//    CPU USAGE PREDICTION VARIABLES    //

	typedef BoostAcc::accumulator_set<double, BoostAcc::stats<BoostAccTag::sum, BoostAccTag::mean> > accumulator_t;

	//! Accumulator to keep track of the accuracy in predictions
	static accumulator_t _cpuUsageAccuracyAccum;

	//! The most recent past CPU usage prediction
	static double _cpuUsagePrediction;

	//! Whether a prediciton has been done
	static bool _cpuUsageAvailable;

private:

	//! \brief Display monitoring statistics
	static void displayStatistics();

	//! \brief Try to load previous monitoring data into accumulators
	static void loadMonitoringWisdom();

	//! \brief Store monitoring data for future executions as warmup data
	static void storeMonitoringWisdom();

public:

	//    MONITORING    //

	//! \brief Pre-initialize monitoring structures before CPU structures are
	//! initialized (see Bootstrap.cpp)
	static void preinitialize();

	//! \brief Initialize monitoring structures after CPU structures are
	//! initialized (see Bootstrap.cpp)
	static void initialize();

	//! \brief Shutdown monitoring
	static void shutdown();

	//! \brief Check whether monitoring is enabled
	static inline bool isEnabled()
	{
		return _enabled;
	}

	//    TASKS    //

	//! \brief Gather basic information about a task when it is created
	//!
	//! \param[in,out] task The task to gather information about
	static void taskCreated(Task *task);

	//! \brief Propagate monitoring operations after a task has changed its
	//! execution status
	//!
	//! \param[in,out] task The task that's changing status
	//! \param[in] newStatus The new execution status of the task
	static void taskChangedStatus(Task *task, monitoring_task_status_t newStatus);

	//! \brief Propagate monitoring operations after a task has
	//! completed user code execution
	//!
	//! \param[in,out] task The task that has completed the execution
	static void taskCompletedUserCode(Task *task);

	//! \brief Propagate monitoring operations after a task has finished
	//!
	//! \param[in,out] task The task that has finished
	static void taskFinished(Task *task);

	//! \brief Get the size needed to create a TaskStatistics object
	//!
	//! \return TaskStatistics size or 0 if Monitoring is disabled
	static inline size_t getTaskStatisticsSize()
	{
		if (_enabled) {
			return sizeof(TaskStatistics);
		}

		return 0;
	}

	//    CPUS    //

	//! \brief Propagate monitoring operations when a CPU becomes idle
	//!
	//! \param[in] cpuId The identifier of the CPU
	static void cpuBecomesIdle(int cpuId);

	//! \brief Propagate monitoring operations when a CPU becomes active
	//!
	//! \param[in] cpuId The identifier of the CPU
	static void cpuBecomesActive(int cpuId);

	//    PREDICTORS    //

	//! \brief Get a timing prediction of a certain workload
	//!
	//! \param[in] loadId The workload's id
	static double getPredictedWorkload(workload_t loadId);

	//! \brief Get a CPU Usage prediction over an amount of time
	//!
	//! \param[in] time The amount of time in microseconds to predict usage for
	//! (i.e. in the next 'time' microseconds, the amount of CPUs to be used)
	//!
	//! \return The expected CPU Usage for the next 'time' microseconds
	static double getCPUUsagePrediction(size_t time);

	//! \brief Poll the expected time until completion of the current execution
	//!
	//! \return An estimation of the time to completion in microseconds
	static double getPredictedElapsedTime();

};

#endif // MONITORING_HPP
