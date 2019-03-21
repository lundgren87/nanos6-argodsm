#include <cstdlib>
#include <vector>

#include "ArgoDataTransfer.hpp"
#include "ArgoMessenger.hpp"
#include "lowlevel/EnvironmentVariable.hpp"
#include "cluster/messages/Message.hpp"
#include "cluster/polling-services/ClusterPollingServices.hpp"
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
	int ret;
        unsigned long distribSize;
	
        EnvironmentVariable<StringifiedMemorySize> distribSizeEnv("ARGO_DISTRIBUTED_MEMORY", (1UL << 29));
	distribSize = distribSizeEnv.getValue();
	argo::init(distribSize);
	printf("Node %d: ArgoDSM initialized with %lu memory.\n", argo::node_id(), argo::backend::global_size());
	
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
	
	ret = MPI_Comm_rank(INTRA_COMM, &_wrank);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	ret = MPI_Comm_size(INTRA_COMM, &_wsize);
	MPIErrorHandler::handle(ret, INTRA_COMM);
}

ArgoMessenger::~ArgoMessenger()
{
	int ret;
	
	//! Release the intra-communicator
	ret = MPI_Comm_free(&INTRA_COMM);
	MPIErrorHandler::handle(ret, INTRA_COMM);

        argo::finalize();
}

void ArgoMessenger::sendMessage(Message *msg, ClusterNode const *toNode, bool block)
{
	int ret;
	Message::Deliverable *delv = msg->getDeliverable();
	const int mpiDst = toNode->getCommIndex();
	size_t msgSize = sizeof(delv->header) + delv->header.size;
	
	//! At the moment we use the Message id and the Message type to create
	//! the MPI tag of the communication
	int tag = (delv->header.id << 8) | delv->header.type;

	assert(mpiDst < _wsize && mpiDst != _wrank);
	assert(delv->header.size != 0);
	
	if (block) {
		ret = MPI_Send((void *)delv, msgSize, MPI_BYTE, mpiDst,
				tag, INTRA_COMM);
		MPIErrorHandler::handle(ret, INTRA_COMM);
		
		msg->markAsDelivered();
		return;
	}
	
	MPI_Request *request =
		(MPI_Request *)MemoryAllocator::alloc(
				sizeof(MPI_Request));
	FatalErrorHandler::failIf(
		request == nullptr,
		"Could not allocate memory for MPI_Request"
	);
	
	ret = MPI_Isend((void *)delv, msgSize, MPI_BYTE, mpiDst,
			tag, INTRA_COMM, request);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	msg->setMessengerData((void *)request);
	ClusterPollingServices::addPendingMessage(msg);
}

DataTransfer *ArgoMessenger::sendData(const DataAccessRegion &region,
		const ClusterNode *to, int messageId, bool block)
{
	int ret, tag;
	const int mpiDst = to->getCommIndex();
	void *address = region.getStartAddress();
	size_t size = region.getSize();
	
	assert(mpiDst < _wsize && mpiDst != _wrank);
	
	tag = (messageId << 8) | DATA_RAW;
	
	if (block) {
		ret = MPI_Send(address, size, MPI_BYTE, mpiDst, tag,
				INTRA_COMM);
		MPIErrorHandler::handle(ret, INTRA_COMM);
		
		return nullptr;
	}
	
	MPI_Request *request = (MPI_Request *)MemoryAllocator::alloc(sizeof(MPI_Request));
	FatalErrorHandler::failIf(
		request == nullptr,
		"Could not allocate memory for MPI_Request"
	);
	
	ret = MPI_Isend(address, size, MPI_BYTE, mpiDst, tag, INTRA_COMM,
			request);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	return new ArgoDataTransfer(region, ClusterManager::getCurrentMemoryNode(),
			to->getMemoryNode(), request);
}

DataTransfer *ArgoMessenger::fetchData(const DataAccessRegion &region,
		const ClusterNode *from, int messageId, bool block)
{
	int ret, tag;
	const int mpiSrc = from->getCommIndex();
	void *address = region.getStartAddress();
	size_t size = region.getSize();
	
	assert(mpiSrc < _wsize && mpiSrc != _wrank);
	
	tag = (messageId << 8) | DATA_RAW;
	
	if (block) {
		ret = MPI_Recv(address, size, MPI_BYTE, mpiSrc, tag,
				INTRA_COMM, MPI_STATUS_IGNORE);
		MPIErrorHandler::handle(ret, INTRA_COMM);
		
		return nullptr;
	}
	
	MPI_Request *request = (MPI_Request *)MemoryAllocator::alloc(sizeof(MPI_Request));
	FatalErrorHandler::failIf(
		request == nullptr,
		"Could not allocate memory for MPI_Request"
	);
	
	ret = MPI_Irecv(address, size, MPI_BYTE, mpiSrc, tag, INTRA_COMM,
			request);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	return new ArgoDataTransfer(region, from->getMemoryNode(),
			ClusterManager::getCurrentMemoryNode(), request);
}

void ArgoMessenger::synchronizeAll(void)
{
	int ret = MPI_Barrier(INTRA_COMM);
	MPIErrorHandler::handle(ret, INTRA_COMM);
}

Message *ArgoMessenger::checkMail(void)
{
	int ret, flag, count, type;
	MPI_Status status;
	Message::Deliverable *msg;
	
	ret = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, INTRA_COMM, &flag, &status);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	if (!flag) {
		return nullptr;
	}
	
	//! DATA_RAW type of messages will be received by matching 'fetchData'
	//! methods
	type = status.MPI_TAG & 0xff;
	if (type == DATA_RAW) {
		return nullptr;
	}
	
	ret = MPI_Get_count(&status, MPI_BYTE, &count);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	msg = (Message::Deliverable *)malloc(count);
	if (!msg) {
		perror("malloc for message");
		MPI_Abort(INTRA_COMM, 1);
	}
	
	assert(count != 0);
	ret = MPI_Recv((void *)msg, count, MPI_BYTE, status.MPI_SOURCE,
			status.MPI_TAG, INTRA_COMM, MPI_STATUS_IGNORE);
	MPIErrorHandler::handle(ret, INTRA_COMM);
	
	return GenericFactory<int, Message*, Message::Deliverable*>::getInstance().create(type, msg);
}

void ArgoMessenger::testMessageCompletion(
	std::vector<Message *> &messages
) {
	assert(!messages.empty());
	
	int msgCount = messages.size(), ret, completedCount;
	MPI_Request requests[msgCount];
	int finished[msgCount];
	MPI_Status status[msgCount];
	
	for (int i = 0; i < msgCount; ++i) {
		Message *msg = messages[i];
		assert(msg != nullptr);
		
		MPI_Request *req =
			(MPI_Request *)msg->getMessengerData();
		assert(req != nullptr);
		
		requests[i] = *req;
	}
	
	ret = MPI_Testsome(msgCount, requests, &completedCount,
			finished, status);
	MPIErrorHandler::handleErrorInStatus(ret, status, completedCount, INTRA_COMM);
	
	for (int i = 0; i < completedCount; ++i) {
		int index = finished[i];
		Message *msg = messages[index];
		
		msg->markAsDelivered();
		MPI_Request *req = (MPI_Request *)msg->getMessengerData();
		MemoryAllocator::free(req, sizeof(MPI_Request));
	}
}

void ArgoMessenger::testDataTransferCompletion(
	std::vector<DataTransfer *> &transfers
) {
	assert(!transfers.empty());
	
	int msgCount = transfers.size(), ret, completedCount;
	MPI_Request requests[msgCount];
	int finished[msgCount];
	MPI_Status status[msgCount];
	
	for (int i = 0; i < msgCount; ++i) {
		ArgoDataTransfer *dt = (ArgoDataTransfer *)transfers[i];
		assert(dt != nullptr);
		
		MPI_Request *req = dt->getMPIRequest();
		assert(req != nullptr);
		
		requests[i] = *req;
	}
	
	ret = MPI_Testsome(msgCount, requests, &completedCount, finished,
			status);
	MPIErrorHandler::handleErrorInStatus(ret, status, completedCount, INTRA_COMM);
	
	for (int i = 0; i < completedCount; ++i) {
		int index = finished[i];
		ArgoDataTransfer *dt = (ArgoDataTransfer *)transfers[index];
		
		dt->markAsCompleted();
		MPI_Request *req = dt->getMPIRequest();
		MemoryAllocator::free(req, sizeof(MPI_Request));
	}
}
