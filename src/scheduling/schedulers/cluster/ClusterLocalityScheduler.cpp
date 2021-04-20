/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include <vector>
#include <cmath>

#include "ClusterLocalityScheduler.hpp"
#include "memory/directory/Directory.hpp"
#include "system/RuntimeInfo.hpp"
#include "tasks/Task.hpp"

#include <ClusterManager.hpp>
#include <DataAccessRegistrationImplementation.hpp>
#include <ExecutionWorkflow.hpp>
#include <VirtualMemoryManagement.hpp>

#include <argo/argo.hpp>

int ClusterLocalityScheduler::getScheduledNode(
	Task *task,
	ComputePlace *computePlace  __attribute__((unused)),
	ReadyTaskHint hint  __attribute__((unused))
) {
	const size_t clusterSize = ClusterManager::clusterSize();

	std::vector<size_t> bytes(clusterSize, 0);
	bool canBeOffloaded = true;

	DataAccessRegistration::processAllDataAccesses(
		task,
		[&](const DataAccess *access) -> bool {
			const MemoryPlace *location = access->getLocation();
			if (location == nullptr) {
				assert(access->isWeak());
				location = Directory::getDirectoryMemoryPlace();
			}

			DataAccessRegion region = access->getAccessRegion();
			if (!VirtualMemoryManagement::isClusterMemory(region) &&
				!argo::is_argo_address(region.getStartAddress())) {
				canBeOffloaded = false;
				return false;
			}

			if (Directory::isDirectoryMemoryPlace(location)) {
				const Directory::HomeNodesArray *homeNodes = Directory::find(region);

				for (const auto &entry : *homeNodes) {
					location = entry->getHomeNode();
					DataAccessRegion subregion = region.intersect(entry->getAccessRegion());
					size_t nodeId = 0;
					//! If the subregion is in argo memory
					if (argo::is_argo_address(subregion.getStartAddress())) {
						char* startAddress = static_cast<char*>(subregion.getStartAddress());
						int chunks = 0;
						size_t chunk_size = argo::get_block_size();
						for(char* addr = startAddress;
								addr < startAddress+subregion.getSize();
								addr += chunk_size) {
							nodeId += static_cast<size_t>(
									argo::get_homenode(static_cast<void*>(addr)));
							chunks++;
						}
						assert(chunks>0);
						size_t avg_nodeId = std::lround(
								static_cast<double>(nodeId)/static_cast<double>(chunks));
						bytes[avg_nodeId] += subregion.getSize();
					} else {
						nodeId = getNodeIdForLocation(location);

						bytes[nodeId] += subregion.getSize();
					}
				}

				delete homeNodes;
			} else {
				size_t nodeId = 0;
				if (argo::is_argo_address(region.getStartAddress())) {
					char* startAddress = static_cast<char*>(region.getStartAddress());
					int chunks = 0;
					size_t chunk_size = argo::get_block_size();
					for(char* addr = startAddress;
							addr < startAddress+region.getSize();
							addr += chunk_size) {
						nodeId += static_cast<size_t>(
								argo::get_homenode(static_cast<void*>(addr)));
						chunks++;
					}
					assert(chunks>0);
					size_t avg_nodeId =
						std::lround(static_cast<double>(nodeId)/static_cast<double>(chunks));
					bytes[avg_nodeId] += region.getSize();
				} else {
					size_t nodeId = getNodeIdForLocation(location);

					bytes[nodeId] += region.getSize();
				}
			}

			return true;
		}
	);

	if (!canBeOffloaded) {
		return nanos6_cluster_no_offload;
	}

	assert(!bytes.empty());
	std::vector<size_t>::iterator it = bytes.begin();
	const size_t nodeId = std::distance(it, std::max_element(it, it + clusterSize));

	return nodeId;
}
