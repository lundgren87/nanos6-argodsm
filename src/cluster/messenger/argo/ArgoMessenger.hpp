/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
	
	Copyright (C) 2018 Barcelona Supercomputing Center (BSC)
*/

#ifndef ARGO_MESSENGER_HPP
#define ARGO_MESSENGER_HPP

#include <sstream>
#include <vector>

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

#include "../Messenger.hpp"

class ClusterPlace;
class DataTransfer;
class Message;

class ArgoMessenger : public Messenger {
private:
	int _wrank, _wsize;
	MPI_Comm INTRA_COMM, PARENT_COMM;
	
public:
	ArgoMessenger();
	~ArgoMessenger();
	
	void sendMessage(Message *msg, ClusterNode const *toNode, bool block = false);
	void synchronizeAll(void);
	DataTransfer *sendData(const DataAccessRegion &region, const ClusterNode *toNode, int messageId, bool block);
	DataTransfer *fetchData(const DataAccessRegion &region, const ClusterNode *fromNode, int messageId, bool block);
	Message *checkMail();
	void testMessageCompletion(std::vector<Message *> &messages);
	void testDataTransferCompletion(std::vector<DataTransfer *> &transfers);
	
	inline int getNodeIndex() const
	{
		return _wrank;
	}
	
	inline int getMasterIndex() const
	{
		return 0;
	}
	
	inline int getClusterSize() const
	{
		return _wsize;
	}
	
	inline bool isMasterNode() const
	{
		return _wrank == 0;
	}
};

//! Register ArgoMessenger with the object factory
namespace
{
	Messenger *createArgoMsn() { return new ArgoMessenger; }
	
	const bool __attribute__((unused))_registered_Argo_msn =
		REGISTER_MSN_CLASS("argo", createArgoMsn);
}

#endif /* ARGO_MESSENGER_HPP */
