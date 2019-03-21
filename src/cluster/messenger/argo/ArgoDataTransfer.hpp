#ifndef ARGO_DATA_TRANSFER_HPP
#define ARGO_DATA_TRANSFER_HPP

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

#include "../DataTransfer.hpp"

class ArgoDataTransfer : public DataTransfer {
	//! MPI_Request object related with the pending data transfer.
	MPI_Request *_request;
	
public:
	ArgoDataTransfer(
		DataAccessRegion const &region,
		MemoryPlace const *source,
		MemoryPlace const *target,
		MPI_Request *request
	) : DataTransfer(region, source, target), _request(request)
	{
	}
	
	~ArgoDataTransfer()
	{
	}
	
	//! \brief Get the MPI_Request object of the MPIDataTransfer
	inline MPI_Request *getMPIRequest() const
	{
		return _request;
	}
};

#endif /* ARGO_DATA_TRANSFER_HPP */
