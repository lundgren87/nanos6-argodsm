#ifndef ARGO_DATA_TRANSFER_HPP
#define ARGO_DATA_TRANSFER_HPP

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

#include "../DataTransfer.hpp"

class ArgoDataTransfer : public DataTransfer {
	
public:
	ArgoDataTransfer(
		DataAccessRegion const &region,
		MemoryPlace const *source,
		MemoryPlace const *target,
		MPI_Request *request,
		int MPISource,
		int transferId,
		bool isFetch
	) : DataTransfer(region, source, target, request, MPISource, transferId, isFetch)
	{
	}
	
	~ArgoDataTransfer()
	{
	}
};

#endif /* ARGO_DATA_TRANSFER_HPP */
