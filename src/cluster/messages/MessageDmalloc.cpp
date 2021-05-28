/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#include "MessageDmalloc.hpp"

#include <ClusterManager.hpp>
#include <ClusterMemoryManagement.hpp>
#include <DistributionPolicy.hpp>
#include <VirtualMemoryManagement.hpp>

#include <argo/argo.hpp>
#include "lowlevel/EnvironmentVariable.hpp"
#include "system/RuntimeInfo.hpp"

MessageDmalloc::MessageDmalloc(const ClusterNode *from, size_t numDimensions)
	: Message(DMALLOC,
		sizeof(DmallocMessageContent) + numDimensions * sizeof(size_t),
		from)
{
	_content = reinterpret_cast<DmallocMessageContent *>(_deliverable->payload);
}

bool MessageDmalloc::handleMessage()
{
	void *dptr = nullptr;
	size_t size = getAllocationSize();
	nanos6_data_distribution_t policy = getDistributionPolicy();
	size_t nrDim = getDimensionsSize();
	size_t *dimensions = getDimensions();
	if (ClusterManager::isMasterNode()) {
		/* The master node performs the allocation and communicates
		 * the allocated address to all other nodes */

		// Get communicator type
		ConfigVariable<std::string> commType("cluster.communication");

		// Allocate ArgoDSM memory if selected
		if(commType.getValue() == "argodsm"){
			printf("Allocating %zu ArgoDSM distributed memory.\n", size);
			dptr = dynamic_alloc(size);
		}else{
			printf("Allocating %zu Nanos6 distributed memory.\n", size);
			dptr = VirtualMemoryManagement::allocDistrib(size);
		}

		DataAccessRegion address(&dptr, sizeof(void *));

		ClusterNode *current = ClusterManager::getCurrentClusterNode();
		assert(current != nullptr);

		std::vector<ClusterNode *> const &world =
			ClusterManager::getClusterNodes();

		/* Send the allocated address to everyone else */
		for (ClusterNode *node : world) {
			if (node == current) {
				continue;
			}

			ClusterMemoryNode *memoryNode = node->getMemoryNode();
			ClusterManager::sendDataRaw(address, memoryNode, getId(), true);
		}
	} else {
		/* This is a slave node. We will receive the allocated address
		 * from the master node */
		DataAccessRegion address(&dptr, sizeof(void *));

		ClusterNode *masterNode = ClusterManager::getMasterNode();
		ClusterMemoryNode *masterMemoryNode = masterNode->getMemoryNode();

		ClusterManager::fetchDataRaw(address, masterMemoryNode, getId(), true);
	}

	/* Register the newly allocated region with the Directory
	 * of home nodes */
	DataAccessRegion allocatedRegion(dptr, size);
	ClusterDirectory::registerAllocation(allocatedRegion, policy, nrDim, dimensions, nullptr);

	ClusterManager::synchronizeAll();

	return true;
}

static const bool __attribute__((unused))_registered_dmalloc =
	Message::RegisterMSGClass<MessageDmalloc>(DMALLOC);
