/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include <cstdlib>
#include <vector>

#include "InstrumentCluster.hpp"
#include "ArgoDataTransfer.hpp"
#include "ArgoMessenger.hpp"
#include "cluster/messages/Message.hpp"
#include "cluster/polling-services/ClusterServicesPolling.hpp"
#include "cluster/polling-services/ClusterServicesTask.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "lowlevel/mpi/MPIErrorHandler.hpp"

#include <ClusterManager.hpp>
#include <ClusterNode.hpp>
#include <MemoryAllocator.hpp>

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

#include "argo/argo.hpp"

ArgoMessenger::ArgoMessenger()
{
	int ret, ubIsSetFlag;
	std::size_t distribSize, cacheSize;

	//! Get the requested ArgoDSM distributed memory and cache sizes
	ConfigVariable<StringifiedMemorySize> distribSizeConfig("argodsm.distributed_memory");
	distribSize = distribSizeConfig.getValue();
	ConfigVariable<StringifiedMemorySize> cacheSizeConfig("argodsm.cache_size");
	cacheSize = cacheSizeConfig.getValue();
	cacheSize = (cacheSize > 0) ? cacheSize : distribSize;

	//! Initialize ArgoDSM and print confirmation
	argo::init(distribSize, cacheSize);
	printf("Node %d: ArgoDSM initialized with %luMb distributed memory.\n", argo::node_id(), argo::backend::global_size()/(1024*1024));
	
	//! make sure that MPI errors are returned in the COMM_WORLD
	ret = MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
	MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
	
	//! Save the parent communicator
	ret = MPI_Comm_get_parent(&PARENT_COMM);
	MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
	
	//! Create a new communicator
	ret = MPI_Comm_dup(MPI_COMM_WORLD, &INTRA_COMM);
	MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
	
	//! make sure the new communicator returns errors
	ret = MPI_Comm_set_errhandler(INTRA_COMM, MPI_ERRORS_RETURN);
	MPIErrorHandler::handle(ret, INTRA_COMM);

	//! make sure the new communicator returns errors
	ret = MPI_Comm_set_errhandler(INTRA_COMM, MPI_ERRORS_RETURN);
	MPIErrorHandler::handle(ret, INTRA_COMM);

	ConfigVariable<bool> mpi_comm_data_raw("cluster.mpi.comm_data_raw");
	_mpi_comm_data_raw = mpi_comm_data_raw.getValue();

	if (_mpi_comm_data_raw) {
		ret = MPI_Comm_dup(INTRA_COMM, &INTRA_COMM_DATA_RAW);
		MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
	} else {
		INTRA_COMM_DATA_RAW = INTRA_COMM;
	}

	ret = MPI_Comm_rank(INTRA_COMM, &_wrank);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	ret = MPI_Comm_size(INTRA_COMM, &_wsize);
	MPIErrorHandler::handle(ret, INTRA_COMM);

	//! Get the upper-bound tag supported by current MPI implementation
	int *mpi_ub_tag = nullptr;
	ret = MPI_Comm_get_attr(INTRA_COMM, MPI_TAG_UB, &mpi_ub_tag, &ubIsSetFlag);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	assert(mpi_ub_tag != nullptr);
	assert(ubIsSetFlag != 0);

	_mpi_ub_tag = *mpi_ub_tag;
	assert(_mpi_ub_tag > 0);
}

ArgoMessenger::~ArgoMessenger()
{
	int ret;
	if (_mpi_comm_data_raw ==  true) {
#ifndef NDEBUG
		int compare = 0;
		ret = MPI_Comm_compare(INTRA_COMM_DATA_RAW, INTRA_COMM, &compare);
		MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
		assert(compare !=  MPI_IDENT);
#endif // NDEBUG

		//! Release the INTRA_COMM_DATA_RAW
		ret = MPI_Comm_free(&INTRA_COMM_DATA_RAW);
		MPIErrorHandler::handle(ret, MPI_COMM_WORLD);
	}

	//! Release the intra-communicator
	ret = MPI_Comm_free(&INTRA_COMM);
	MPIErrorHandler::handle(ret, INTRA_COMM);

	argo::finalize();

	RequestContainer<Message>::clear();
	RequestContainer<DataTransfer>::clear();
}

void ArgoMessenger::sendMessage(Message *msg, ClusterNode const *toNode, bool block)
{
	int ret;
	Message::Deliverable *delv = msg->getDeliverable();
	const int mpiDst = toNode->getCommIndex();
	size_t msgSize = sizeof(delv->header) + delv->header.size;
	
	//! At the moment we use the Message id and the Message type to create
	//! the MPI tag of the communication
	int tag = createTag(delv);

	assert(mpiDst < _wsize && mpiDst != _wrank);
	assert(delv->header.size != 0);
	
	Instrument::clusterSendMessage(msg, mpiDst);

	if (block) {
		ExtraeLock();
		ret = MPI_Send((void *)delv, msgSize, MPI_BYTE, mpiDst, tag, INTRA_COMM);
		ExtraeUnlock();
		MPIErrorHandler::handle(ret, INTRA_COMM);

		// Note: instrument before mark as completed, otherwise possible use-after-free
		Instrument::clusterSendMessage(msg, -1);
		msg->markAsCompleted();
		return;
	}
	
	MPI_Request *request = (MPI_Request *)MemoryAllocator::alloc(sizeof(MPI_Request));
	FatalErrorHandler::failIf(request == nullptr, "Could not allocate memory for MPI_Request");

	ExtraeLock();
	ret = MPI_Isend((void *)delv, msgSize, MPI_BYTE, mpiDst, tag, INTRA_COMM, request);
	ExtraeUnlock();

	MPIErrorHandler::handle(ret, INTRA_COMM);

	msg->setMessengerData((void *)request);

	// Note instrument before add as pending, otherwise can be processed and freed => use-after-free
	Instrument::clusterSendMessage(msg, -1);
	ClusterPollingServices::PendingQueue<Message>::addPending(msg);
}

DataTransfer *ArgoMessenger::sendData(
	const DataAccessRegion &region,
	const ClusterNode *to,
	int messageId,
	bool block,
	bool instrument
) {
	int ret;
	const int mpiDst = to->getCommIndex();
	void *address = region.getStartAddress();

	const size_t size = region.getSize();

	assert(mpiDst < _wsize && mpiDst != _wrank);

	if (instrument) {
		Instrument::clusterDataSend(address, size, mpiDst, messageId);
	}

	int tag = getTag(messageId);

	if (block) {
		ExtraeLock();
		ret = MPI_Send(address, size, MPI_BYTE, mpiDst, tag, INTRA_COMM_DATA_RAW);
		ExtraeUnlock();
		MPIErrorHandler::handle(ret, INTRA_COMM_DATA_RAW);

		return nullptr;
	}

	MPI_Request *request = (MPI_Request *)MemoryAllocator::alloc(sizeof(MPI_Request));

	FatalErrorHandler::failIf(request == nullptr, "Could not allocate memory for MPI_Request");

	ExtraeLock();
	ret = MPI_Isend(address, size, MPI_BYTE, mpiDst, tag, INTRA_COMM_DATA_RAW, request);
	ExtraeUnlock();
	MPIErrorHandler::handle(ret, INTRA_COMM_DATA_RAW);

	if (instrument) {
		Instrument::clusterDataSend(NULL, 0, mpiDst, -1);
	}

	return new ArgoDataTransfer(region, ClusterManager::getCurrentMemoryNode(),
		to->getMemoryNode(), request, mpiDst, messageId, /* isFetch */ false);
}

DataTransfer *ArgoMessenger::fetchData(
	const DataAccessRegion &region,
	const ClusterNode *from,
	int messageId,
	bool block,
	bool instrument
) {
	int ret;
	const int mpiSrc = from->getCommIndex();
	void *address = region.getStartAddress();
	size_t size = region.getSize();

	assert(mpiSrc < _wsize && mpiSrc != _wrank);

	int tag = getTag(messageId);

	if (block) {
		ExtraeLock();
		ret = MPI_Recv(address, size, MPI_BYTE, mpiSrc, tag, INTRA_COMM_DATA_RAW, MPI_STATUS_IGNORE);
		ExtraeUnlock();
		MPIErrorHandler::handle(ret, INTRA_COMM_DATA_RAW);
		if (instrument) {
			Instrument::clusterDataReceived(address, size, mpiSrc, messageId);
		}

		return nullptr;
	}

	MPI_Request *request = (MPI_Request *)MemoryAllocator::alloc(sizeof(MPI_Request));

	FatalErrorHandler::failIf(request == nullptr, "Could not allocate memory for MPI_Request");

	ExtraeLock();
	ret = MPI_Irecv(address, size, MPI_BYTE, mpiSrc, tag, INTRA_COMM_DATA_RAW, request);
	ExtraeUnlock();

	MPIErrorHandler::handle(ret, INTRA_COMM_DATA_RAW);

	return new ArgoDataTransfer(region, from->getMemoryNode(),
		ClusterManager::getCurrentMemoryNode(), request, mpiSrc, messageId, /* isFetch */ true);
}

void ArgoMessenger::synchronizeAll(void)
{
	ExtraeLock();
	int ret = MPI_Barrier(INTRA_COMM);
	ExtraeUnlock();
	MPIErrorHandler::handle(ret, INTRA_COMM);
}

Message *ArgoMessenger::checkMail(void)
{
	int ret, flag, count;
	MPI_Status status;

	ExtraeLock();
	ret = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, INTRA_COMM, &flag, &status);
	ExtraeUnlock();
	MPIErrorHandler::handle(ret, INTRA_COMM);

	if (!flag) {
		return nullptr;
	}

	//! DATA_RAW type of messages will be received by matching 'fetchData'
	//! methods
	const int type = status.MPI_TAG & 0xff;
	if (type == DATA_RAW) {
		std::cout << "give up checkMail for DATA_RAW\n";
		return nullptr;
	}

	ret = MPI_Get_count(&status, MPI_BYTE, &count);
	MPIErrorHandler::handle(ret, INTRA_COMM);

	Message::Deliverable *msg = (Message::Deliverable *) malloc(count);
	if (msg == nullptr) {
		perror("malloc for message");
		MPI_Abort(INTRA_COMM, 1);
	}

	assert(count != 0);
	ExtraeLock();
	ret = MPI_Recv((void *)msg, count, MPI_BYTE, status.MPI_SOURCE,
		status.MPI_TAG, INTRA_COMM, MPI_STATUS_IGNORE);
	ExtraeUnlock();
	MPIErrorHandler::handle(ret, INTRA_COMM);

	return GenericFactory<int, Message*, Message::Deliverable*>::getInstance().create(type, msg);
}

template <typename T>
void ArgoMessenger::testCompletionInternal(std::vector<T *> &pendings)
{
	const size_t msgCount = pendings.size();
	assert(msgCount > 0);

	int completedCount;

	RequestContainer<T>::reserve(msgCount);
	assert(RequestContainer<T>::requests != nullptr);
	assert(RequestContainer<T>::finished != nullptr);
	assert(RequestContainer<T>::status != nullptr);

	for (size_t i = 0; i < msgCount; ++i) {
		T *msg = pendings[i];
		assert(msg != nullptr);

		MPI_Request *req = (MPI_Request *)msg->getMessengerData();
		assert(req != nullptr);

		RequestContainer<T>::requests[i] = *req;
	}

	ExtraeLock();
	const int ret = MPI_Testsome(
		(int) msgCount,
		RequestContainer<T>::requests,
		&completedCount,
		RequestContainer<T>::finished,
		RequestContainer<T>::status
	);
	ExtraeUnlock();

	MPIErrorHandler::handleErrorInStatus(ret, RequestContainer<T>::status, completedCount, INTRA_COMM);

	for (int i = 0; i < completedCount; ++i) {
		const int index = RequestContainer<T>::finished[i];
		T *msg = pendings[index];

		msg->markAsCompleted();
		MPI_Request *req = (MPI_Request *) msg->getMessengerData();
		MemoryAllocator::free(req, sizeof(MPI_Request));
	}
}
