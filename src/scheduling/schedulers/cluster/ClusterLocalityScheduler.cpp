/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

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
	std::size_t ft_bytes = 0;
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
					DataAccessRegion subregion = region.intersect(entry->getAccessRegion());
					//! If the subregion is in argo memory
					if (argo::is_argo_address(subregion.getStartAddress())) {
						char* startAddress = static_cast<char*>(subregion.getStartAddress());
						char* endAddress = startAddress + subregion.getSize();
						size_t block_size = argo::get_block_size();
						for(char* addr = startAddress;
								addr < endAddress;
								addr += block_size) {
							int homenode = argo::get_homenode(addr);
							if(homenode < 0) {
								//! Chunk has not been first-touched
								ft_bytes += (addr+block_size < endAddress) ?
									block_size : endAddress - addr;
							} else {
								//! Chunk is backed on one node
								bytes[homenode] += (addr+block_size < endAddress) ? 
									block_size : endAddress - addr;
							}
						}
					} else {
						location = entry->getHomeNode();
						size_t nodeId = getNodeIdForLocation(location);

						bytes[nodeId] += subregion.getSize();
					}
				}

				delete homeNodes;
			} else {
				if (argo::is_argo_address(region.getStartAddress())) {
					char* startAddress = static_cast<char*>(region.getStartAddress());
					char* endAddress = startAddress + region.getSize();
					size_t block_size = argo::get_block_size();
					for(char* addr = startAddress;
							addr < endAddress;
							addr += block_size) {
						int homenode = argo::get_homenode(addr);
						if(homenode < 0) {
							//! Chunk has not been first-touched
							ft_bytes += (addr+block_size < endAddress) ? 
								block_size : endAddress - addr;
						} else {
							//! Chunk is backed on one node
							bytes[homenode] += (addr+block_size < endAddress) ? 
								block_size : endAddress - addr;
						}
					}
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

	size_t nodeId;
	assert(!bytes.empty());
	//const size_t total_bytes = std::accumulate(bytes.begin(), bytes.end(), 0);
	const size_t max_bytes = *std::max_element(bytes.begin(), bytes.end());

	//std::cout << "picking from: [";
	//for(auto v : bytes) {
	//	std::cout << v << " ";
	//}
	//std::cout << "]" << std::endl;
	
	if(ft_bytes > 2*max_bytes) {
		nodeId = getNextFtNode();
		//printf("Picked next FT node: %zu\n", nodeId);
	} else {
		std::vector<size_t>::iterator it = bytes.begin();
		nodeId = std::distance(it, std::max_element(it, it + clusterSize));
		//printf("Picked non-FT node: %zu\n", nodeId);
	}

	return nodeId;
}
