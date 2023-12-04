//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================


#include <ittnotify.h>
#include <mpi.h>

enum {
    MPI_TASK_SEND,
    MPI_TASK_RECV,
    MPI_TASK_ISEND,
    MPI_TASK_IRECV,
    MPI_TASK_WAIT,
    MPI_TASK_TEST,
    MPI_TASK_WAITANY,
    MPI_TASK_TESTANY,
    MPI_TASK_WAITALL,
    MPI_TASK_TESTALL,
    MPI_TASK_WAITSOME,
    MPI_TASK_TESTSOME,
    MPI_TASK_IPROBE,
    MPI_TASK_PROBE,
    MPI_TASK_SENDRECV,
    MPI_TASK_SENDRECV_REPLACE,
    MPI_TASK_INIT,
    MPI_TASK_INIT_THREAD,
    MPI_TASK_FINALIZE,
    MPI_TASK_BARRIER,
    MPI_TASK_BCAST,
    MPI_TASK_GATHER,
    MPI_TASK_GATHERV,
    MPI_TASK_SCATTER,
    MPI_TASK_SCATTERV,
    MPI_TASK_ALLGATHER,
    MPI_TASK_ALLGATHERV,
    MPI_TASK_ALLTOALL,
    MPI_TASK_ALLTOALLV,
    MPI_TASK_REDUCE,
    MPI_TASK_ALLREDUCE,
    MPI_TASK_REDUCE_SCATTER,
    MPI_TASK_GET,
    MPI_TASK_PUT,
    MPI_TASK_IALLGATHERV,
    MPI_TASK_IALLREDUCE,
    MPI_TASK_IALLTOALL,
    MPI_TASK_IALLTOALLV,
    MPI_TASK_IBARRIER,
    MPI_TASK_IBCAST,
    MPI_TASK_IREDUCE,
    MPI_TASK_IREDUCE_SCATTER_BLOCK,
    MPI_TASK_WIN_CREATE,
    MPI_TASK_WIN_FENCE,
    MPI_TASK_WIN_FREE,
    MPI_TASK_WIN_LOCK,
    MPI_TASK_WIN_UNLOCK,
    MPI_TASK_WIN_LOCK_ALL,
    MPI_TASK_WIN_UNLOCK_ALL,
    MPI_TASK_WIN_FLUSH,
    MPI_TASK_WIN_FLUSH_ALL,
    MPI_TASK_WIN_FLUSH_LOCAL,
    MPI_TASK_WIN_FLUSH_LOCAL_ALL,
    MPI_TASK_WIN_SYNC,
    MPI_TASK_CANCEL,
    MPI_TASK_COMM_CREATE_GROUP,
    MPI_TASK_COMM_FREE,
    MPI_TASK_COMM_GET_ATTR,
    MPI_TASK_COMM_GET_INFO,
    MPI_TASK_COMM_GROUP,
    MPI_TASK_COMM_RANK,
    MPI_TASK_COMM_SET_INFO,
    MPI_TASK_COMM_SIZE,
    MPI_TASK_COMM_SPLIT,
    MPI_TASK_COMM_SPLIT_TYPE,
    MPI_TASK_ERROR_STRING,
    MPI_TASK_FINALIZED,
    MPI_TASK_GET_COUNT,
    MPI_TASK_GET_LIBRARY_VERSION,
    MPI_TASK_GROUP_INCL,
    MPI_TASK_INFO_CREATE,
    MPI_TASK_INFO_FREE,
    MPI_TASK_INFO_GET,
    MPI_TASK_INFO_SET,
    MPI_TASK_INITIALIZED,
    MPI_TASK_OP_CREATE,
    MPI_TASK_OP_FREE,
    MPI_TASK_QUERY_THREAD,
    MPI_TASK_REDUCE_SCATTER_BLOCK,
    MPI_TASK_TYPE_COMMIT,
    MPI_TASK_TYPE_CONTIGUOUS,
    MPI_TASK_TYPE_FREE,
    MPI_TASK_NUM
};

const char *mpi_task_names[MPI_TASK_NUM] = {
    "MPI_Send",
    "MPI_Recv",
    "MPI_Isend",
    "MPI_Irecv",
    "MPI_Wait",
    "MPI_Test",
    "MPI_Waitany",
    "MPI_Testany",
    "MPI_Waitall",
    "MPI_Testall",
    "MPI_Waitsome",
    "MPI_Testsome",
    "MPI_Iprobe",
    "MPI_Probe",
    "MPI_Sendrecv",
    "MPI_Sendrecv_replace",
    "MPI_Init",
    "MPI_Init_thread",
    "MPI_Finalize",
    "MPI_Barrier",
    "MPI_Bcast",
    "MPI_Gather",
    "MPI_Gatherv",
    "MPI_Scatter",
    "MPI_Scatterv",
    "MPI_Allgather",
    "MPI_Allgatherv",
    "MPI_Alltoall",
    "MPI_Alltoallv",
    "MPI_Reduce",
    "MPI_Allreduce",
    "MPI_Reeduce_scatter",
    "MPI_Get",
    "MPI_Put",
    "MPI_Iallgatherv",
    "MPI_Iallreduce",
    "MPI_Ialltoall",
    "MPI_Ialltoallv",
    "MPI_Ibarrier",
    "MPI_Ibcast",
    "MPI_Ireduce",
    "MPI_Ireduce_scatter_block",
    "MPI_Win_create",
    "MPI_Win_fence",
    "MPI_Win_free",
    "MPI_Win_lock",
    "MPI_Win_unlock",
    "MPI_Win_lock_all",
    "MPI_Win_unlock_all",
    "MPI_Win_flush",
    "MPI_Win_flush_all",
    "MPI_Win_flush_local",
    "MPI_Win_flush_local_all",
    "MPI_Win_sync",
    "MPI_Cancel",
    "MPI_Comm_create_group",
    "MPI_Comm_free",
    "MPI_Comm_get_attr",
    "MPI_Comm_get_info",
    "MPI_Comm_group",
    "MPI_Comm_rank",
    "MPI_Comm_set_info",
    "MPI_Comm_size",
    "MPI_Comm_split",
    "MPI_Comm_split_type",
    "MPI_Error_string",
    "MPI_Finalized",
    "MPI_Get_count",
    "MPI_Get_library_version",
    "MPI_Group_incl",
    "MPI_Info_create",
    "MPI_Info_free",
    "MPI_Info_get",
    "MPI_Info_set",
    "MPI_Initialized",
    "MPI_Op_create",
    "MPI_Op_free",
    "MPI_Query_thread",
    "MPI_Reduce_scatter_block",
    "MPI_Type_commit",
    "MPI_Type_contiguous",
    "MPI_Type_free"
};

__itt_domain *mpi_domain = NULL;
__itt_string_handle *mpi_task_handles[MPI_TASK_NUM] = { 0 };

#define ITT_BEGIN(_MPI_ID) { \
	__itt_task_begin(mpi_domain, __itt_null, __itt_null, mpi_task_handles[_MPI_ID]); \
	}

#define ITT_END(_MPI_ID) { \
	__itt_task_end(mpi_domain); \
	}

int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_SEND);
    result = PMPI_Send(buf, count, datatype, dest, tag, comm);
    ITT_END(MPI_TASK_SEND);
    return result;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_RECV);
    result = PMPI_Recv(buf, count, datatype, source, tag, comm, status);
    ITT_END(MPI_TASK_RECV);
    return result;
}

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request * request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ISEND);
    result = PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
    ITT_END(MPI_TASK_ISEND);
    return result;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
              MPI_Comm comm, MPI_Request * request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IRECV);
    result = PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
    ITT_END(MPI_TASK_IRECV);
    return result;
}

int MPI_Wait(MPI_Request * request, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WAIT);
    result = PMPI_Wait(request, status);
    ITT_END(MPI_TASK_WAIT);
    return result;
}

int MPI_Test(MPI_Request * request, int *flag, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TEST);
    result = PMPI_Test(request, flag, status);
    ITT_END(MPI_TASK_TEST);
    return result;
}

int MPI_Waitany(int count, MPI_Request array_of_requests[], int *indx, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WAITANY);
    result = PMPI_Waitany(count, array_of_requests, indx, status);
    ITT_END(MPI_TASK_WAITANY);
    return result;
}

int MPI_Testany(int count, MPI_Request array_of_requests[], int *indx, int *flag,
                MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TESTANY);
    result = PMPI_Testany(count, array_of_requests, indx, flag, status);
    ITT_END(MPI_TASK_TESTANY);
    return result;
}

int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[])
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WAITALL);
    result = PMPI_Waitall(count, array_of_requests, array_of_statuses);
    ITT_END(MPI_TASK_WAITALL);
    return result;
}

int MPI_Testall(int count, MPI_Request array_of_requests[], int *flag,
                MPI_Status array_of_statuses[])
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TESTALL);
    result = PMPI_Testall(count, array_of_requests, flag, array_of_statuses);
    ITT_END(MPI_TASK_TESTALL);
    return result;
}

int MPI_Waitsome(int incount, MPI_Request array_of_requests[], int *outcount,
                 int array_of_indices[], MPI_Status array_of_statuses[])
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WAITSOME);
    result =
        PMPI_Waitsome(incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
    ITT_END(MPI_TASK_WAITSOME);
    return result;
}

int MPI_Testsome(int incount, MPI_Request array_of_requests[], int *outcount,
                 int array_of_indices[], MPI_Status array_of_statuses[])
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TESTSOME);
    result = PMPI_Testsome(incount, array_of_requests, outcount,
                           array_of_indices, array_of_statuses);
    ITT_END(MPI_TASK_TESTSOME);
    return result;
}

int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IPROBE);
    result = PMPI_Iprobe(source, tag, comm, flag, status);
    ITT_END(MPI_TASK_IPROBE);
    return result;
}

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_PROBE);
    result = PMPI_Probe(source, tag, comm, status);
    ITT_END(MPI_TASK_PROBE);
    return result;
}

int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest,
                 int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int source, int recvtag, MPI_Comm comm, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_SENDRECV);
    result = PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest,
                           sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status);
    ITT_END(MPI_TASK_SENDRECV);
    return result;
}

int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype, int dest,
                         int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status * status)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_SENDRECV_REPLACE);
    result = PMPI_Sendrecv_replace(buf, count, datatype, dest,
                                   sendtag, source, recvtag, comm, status);
    ITT_END(MPI_TASK_SENDRECV_REPLACE);
    return result;
}

int MPI_Init(int *argc, char ***argv)
{
    int result = 0;

    mpi_domain = __itt_domain_create("MPI");

    int i = 0;

    for (i = 0; i < MPI_TASK_NUM; i++) {
        mpi_task_handles[i] = __itt_string_handle_create(mpi_task_names[i]);
    }

    ITT_BEGIN(MPI_TASK_INIT);
    result = PMPI_Init(argc, argv);
    ITT_END(MPI_TASK_INIT);
    return result;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    int result = 0;

    mpi_domain = __itt_domain_create("MPI");

    int i = 0;

    for (i = 0; i < MPI_TASK_NUM; i++) {
        mpi_task_handles[i] = __itt_string_handle_create(mpi_task_names[i]);
    }

    ITT_BEGIN(MPI_TASK_INIT_THREAD);
    result = PMPI_Init_thread(argc, argv, required, provided);
    ITT_END(MPI_TASK_INIT_TRHEAD);
    return result;
}

int MPI_Finalize(void)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_FINALIZE);
    result = PMPI_Finalize();
    ITT_END(MPI_TASK_FINALIZE);
    return result;
}

int MPI_Barrier(MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_BARRIER);
    result = PMPI_Barrier(comm);
    ITT_END(MPI_TASK_BARRIER);
    return result;
}

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_BCAST);
    result = PMPI_Bcast(buffer, count, datatype, root, comm);
    ITT_END(MPI_TASK_BCAST);
    return result;
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
               int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GATHER);
    result = PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    ITT_END(MPI_TASK_GATHER);
    return result;
}

int MPI_Gatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                const int *recvcounts, const int *displs, MPI_Datatype recvtype, int root,
                MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GATHERV);
    result = PMPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf,
                          recvcounts, displs, recvtype, root, comm);
    ITT_END(MPI_TASK_GATHERV);
    return result;
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_SCATTER);
    result = PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    ITT_END(MPI_TASK_SCATTER);
    return result;
}

int MPI_Scatterv(const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int root, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_SCATTERV);
    result = PMPI_Scatterv(sendbuf, sendcounts, displs,
                           sendtype, recvbuf, recvcount, recvtype, root, comm);
    ITT_END(MPI_TASK_SCATTERV);
    return result;
}

int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                  int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ALLGATHER);
    result = PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    ITT_END(MPI_TASK_ALLGATHER);
    return result;
}

int MPI_Allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                   const int *recvcounts, const int *displs, MPI_Datatype recvtype, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ALLGATHERV);
    result = PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf,
                             recvcounts, displs, recvtype, comm);
    ITT_END(MPI_TASK_ALLGATHERV);
    return result;
}

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ALLTOALL);
    result = PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    ITT_END(MPI_TASK_ALLTOALL);
    return result;
}

int MPI_Alltoallv(const void *sendbuf, const int *sendcounts,
                  const int *sdispls, MPI_Datatype sendtype, void *recvbuf,
                  const int *recvcounts, const int *rdispls, MPI_Datatype recvtype, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ALLTOALLV);
    result = PMPI_Alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf,
                            recvcounts, rdispls, recvtype, comm);
    ITT_END(MPI_TASK_ALLTOALLV);
    return result;
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, int root, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_REDUCE);
    result = PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    ITT_END(MPI_TASK_REDUCE);
    return result;
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                  MPI_Op op, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ALLREDUCE);
    result = PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    ITT_END(MPI_TASK_ALLREDUCE);
    return result;
}

int MPI_Reduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_REDUCE_SCATTER);
    result = PMPI_Reduce_scatter(sendbuf, recvbuf, recvcounts, datatype, op, comm);
    ITT_END(MPI_TASK_REDUCE_SCATTER);
    return result;
}

int MPI_Get(void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp, int target_count,
            MPI_Datatype target_datatype, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GET);
    result = PMPI_Get(origin_addr, origin_count, origin_datatype,
                      target_rank, target_disp, target_count, target_datatype, win);
    ITT_END(MPI_TASK_GET);
    return result;
}

int MPI_Put(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp, int target_count,
            MPI_Datatype target_datatype, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_PUT);
    result = PMPI_Put(origin_addr, origin_count, origin_datatype,
                      target_rank, target_disp, target_count, target_datatype, win);
    ITT_END(MPI_TASK_PUT);
    return result;
}

int MPI_Iallgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                    const int recvcounts[], const int displs[], MPI_Datatype recvtype,
                    MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IALLGATHERV);
    result = PMPI_Iallgatherv(sendbuf, sendcount, sendtype, recvbuf,
                    recvcounts, displs, recvtype, comm, request);
    ITT_END(MPI_TASK_IALLGATHERV);
    return result;
}

int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op,
                   MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IALLREDUCE);
    result = PMPI_Iallreduce(sendbuf, recvbuf, count, datatype, op, comm, request);
    ITT_END(MPI_TASK_IALLREDUCE);
    return result;
}

int MPI_Ialltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                  int recvcount, MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IALLTOALL);
    result = PMPI_Ialltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, request);
    ITT_END(MPI_TASK_IALLTOALL);
    return result;
}

int MPI_Ialltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                   MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                   const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IALLTOALLV);
    result = PMPI_Ialltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf,
                            recvcounts, rdispls, recvtype, comm, request);
    ITT_END(MPI_TASK_IALLTOALLV);
    return result;
}

int MPI_Ibarrier(MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IBARRIER);
    result = PMPI_Ibarrier(comm, request);
    ITT_END(MPI_TASK_IBARRIER);
    return result;
}

int MPI_Ibcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm,
               MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IBCAST);
    result = PMPI_Ibcast(buffer, count, datatype, root, comm, request);
    ITT_END(MPI_TASK_IBCAST);
    return result;
}

int MPI_Ireduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op,
                int root, MPI_Comm comm, MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IREDUCE);
    result = PMPI_Ireduce(sendbuf, recvbuf, count, datatype, op, root, comm, request);
    ITT_END(MPI_TASK_IREDUCE);
    return result;
}

int MPI_Ireduce_scatter_block(const void *sendbuf, void *recvbuf, int recvcount,
                              MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                              MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_IREDUCE_SCATTER_BLOCK);
    result = PMPI_Ireduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op, comm, request);
    ITT_END(MPI_TASK_IREDUCE_SCATTER_BLOCK);
    return result;
}

int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                   MPI_Win * win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_CREATE);
    result = PMPI_Win_create(base, size, disp_unit, info, comm, win);
    ITT_END(MPI_TASK_WIN_CREATE);
    return result;
}

int MPI_Win_fence(int assert, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FENCE);
    result = PMPI_Win_fence(assert, win);
    ITT_END(MPI_TASK_WIN_FENCE);
    return result;
}

int MPI_Win_free(MPI_Win * win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FREE);
    result = PMPI_Win_free(win);
    ITT_END(MPI_TASK_WIN_FREE);
    return result;
}

int MPI_Win_lock(int lock_type, int rank, int assert, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_LOCK);
    result = PMPI_Win_lock(lock_type, rank, assert, win);
    ITT_END(MPI_TASK_WIN_LOCK);
    return result;
}

int MPI_Win_unlock(int rank, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_UNLOCK);
    result = PMPI_Win_unlock(rank, win);
    ITT_END(MPI_TASK_WIN_UNLOCK);
    return result;
}

int MPI_Win_lock_all(int assert, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_LOCK_ALL);
    result = PMPI_Win_lock_all(assert, win);
    ITT_END(MPI_TASK_WIN_LOCK_ALL);
    return result;
}

int MPI_Win_unlock_all(MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_UNLOCK_ALL);
    result = PMPI_Win_unlock_all(win);
    ITT_END(MPI_TASK_WIN_UNLOCK_ALL);
    return result;
}

int MPI_Win_flush(int rank, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FLUSH);
    result = PMPI_Win_flush(rank, win);
    ITT_END(MPI_TASK_WIN_FLUSH);
    return result;
}

int MPI_Win_flush_all(MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FLUSH_ALL);
    result = PMPI_Win_flush_all(win);
    ITT_END(MPI_TASK_WIN_FLUSH_ALL);
    return result;
}

int MPI_Win_flush_local(int rank, MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FLUSH_LOCAL);
    result = PMPI_Win_flush_local(rank, win);
    ITT_END(MPI_TASK_WIN_FLUSH_LOCAL);
    return result;
}

int MPI_Win_flush_local_all(MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_FLUSH_LOCAL_ALL);
    result = PMPI_Win_flush_local_all(win);
    ITT_END(MPI_TASK_WIN_FLUSH_LOCAL_ALL);
    return result;
}

int MPI_Win_sync(MPI_Win win)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_WIN_SYNC);
    result = PMPI_Win_sync(win);
    ITT_END(MPI_TASK_WIN_SYNC);
    return result;
}

int MPI_Cancel(MPI_Request *request)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_CANCEL);
    result = PMPI_Cancel(request);
    ITT_END(MPI_TASK_CANCEL);
    return result;
}

int MPI_Comm_create_group(MPI_Comm comm, MPI_Group group, int tag, MPI_Comm * newcomm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_CREATE_GROUP);
    result = PMPI_Comm_create_group(comm, group, tag, newcomm);
    ITT_END(MPI_TASK_COMM_CREATE_GROUP);
    return result;
}

int MPI_Comm_free(MPI_Comm *comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_FREE);
    result = PMPI_Comm_free(comm);
    ITT_END(MPI_TASK_COMM_FREE);
    return result;
}

int MPI_Comm_get_attr(MPI_Comm comm, int comm_keyval, void *attribute_val, int *flag)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_GET_ATTR);
    result = PMPI_Comm_get_attr(comm, comm_keyval, attribute_val, flag);
    ITT_END(MPI_TASK_COMM_GET_ATTR);
    return result;
}

int MPI_Comm_get_info(MPI_Comm comm, MPI_Info * info_used)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_GET_INFO);
    result = PMPI_Comm_get_info(comm, info_used);
    ITT_END(MPI_TASK_COMM_GET_INFO);
    return result;
}

int MPI_Comm_group(MPI_Comm comm, MPI_Group * group)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_GROUP);
    result = PMPI_Comm_group(comm, group);
    ITT_END(MPI_TASK_COMM_GROUP);
    return result;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_RANK);
    result = PMPI_Comm_rank(comm, rank);
    ITT_END(MPI_TASK_COMM_RANK);
    return result;
}

int MPI_Comm_set_info(MPI_Comm comm, MPI_Info info)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_SET_INFO);
    result = PMPI_Comm_set_info(comm, info);
    ITT_END(MPI_TASK_COMM_SET_INFO);
    return result;
}

int MPI_Comm_size(MPI_Comm comm, int *size)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_SIZE);
    result = PMPI_Comm_size(comm, size);
    ITT_END(MPI_TASK_COMM_SIZE);
    return result;
}

int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm * newcomm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_SPLIT);
    result = PMPI_Comm_split(comm, color, key, newcomm);
    ITT_END(MPI_TASK_COMM_SPLIT);
    return result;
}

int MPI_Comm_split_type(MPI_Comm comm, int split_type, int key, MPI_Info info, MPI_Comm * newcomm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_COMM_SPLIT_TYPE);
    result = PMPI_Comm_split_type(comm, split_type, key, info, newcomm);
    ITT_END(MPI_TASK_COMM_SPLIT_TYPE);
    return result;
}

int MPI_Error_string(int errorcode, char *string, int *resultlen)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_ERROR_STRING);
    result = PMPI_Error_string(errorcode, string, resultlen);
    ITT_END(MPI_TASK_ERROR_STRING);
    return result;
}

int MPI_Finalized(int *flag)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_FINALIZED);
    result = PMPI_Finalized(flag);
    ITT_END(MPI_TASK_FIANLIZED);
    return result;
}

int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GET_COUNT);
    result = PMPI_Get_count(status, datatype, count);
    ITT_END(MPI_TASK_GET_COUNT);
    return result;
}

int MPI_Get_library_version(char *version, int *resultlen)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GET_LIBRARY_VERSION);
    result = PMPI_Get_library_version(version, resultlen);
    ITT_END(MPI_TASK_GET_LIBRARY_VERSION);
    return result;
}

int MPI_Group_incl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_GROUP_INCL);
    result = PMPI_Group_incl(group, n, ranks, newgroup);
    ITT_END(MPI_TASK_GROUP_INCL);
    return result;
}

int MPI_Info_create(MPI_Info *info)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_INFO_CREATE);
    result = PMPI_Info_create(info);
    ITT_END(MPI_TASK_INFO_CREATE);
    return result;
}

int MPI_Info_free(MPI_Info *info)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_INFO_FREE);
    result = PMPI_Info_free(info);
    ITT_END(MPI_TASK_INFO_FREE);
    return result;
}

int MPI_Info_get(MPI_Info info, const char *key, int valuelen, char *value, int *flag)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_INFO_GET);
    result = PMPI_Info_get(info, key, valuelen, value, flag);
    ITT_END(MPI_TASK_INFO_GET);
    return result;
}

int MPI_Info_set(MPI_Info info, const char *key, const char *value)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_INFO_SET);
    result = PMPI_Info_set(info, key, value);
    ITT_END(MPI_TASK_INFO_SET);
    return result;
}

int MPI_Initialized(int *flag)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_INITIALIZED);
    result = PMPI_Initialized(flag);
    ITT_END(MPI_TASK_INITIALIZED);
    return result;
}

int MPI_Op_create(MPI_User_function *user_fn, int commute, MPI_Op *op)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_OP_CREATE);
    result = PMPI_Op_create(user_fn, commute, op);
    ITT_END(MPI_TASK_OP_CREATE);
    return result;
}

int MPI_Op_free(MPI_Op * op)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_OP_FREE);
    result = PMPI_Op_free(op);
    ITT_END(MPI_TASK_OP_FREE);
    return result;
}

int MPI_Query_thread(int *provided)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_QUERY_THREAD);
    result = PMPI_Query_thread(provided);
    ITT_END(MPI_TASK_QUERY_THREAD);
    return result;
}

int MPI_Reduce_scatter_block(const void *sendbuf, void *recvbuf, int recvcount, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_REDUCE_SCATTER_BLOCK);
    result = PMPI_Reduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op, comm);
    ITT_END(MPI_TASK_REDUCE_SCATTER_BLOCK);
    return result;
}

int MPI_Type_commit(MPI_Datatype *datatype)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TYPE_COMMIT);
    result = PMPI_Type_commit(datatype);
    ITT_END(MPI_TASK_TYPE_COMMIT);
    return result;
}

int MPI_Type_contiguous(int count, MPI_Datatype oldtype, MPI_Datatype * newtype)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TYPE_CONTIGUOUS);
    result = PMPI_Type_contiguous(count, oldtype, newtype);
    ITT_END(MPI_TASK_TYPE_CONTIGUOUS);
    return result;
}

int MPI_Type_free(MPI_Datatype *datatype)
{
    int result = 0;
    ITT_BEGIN(MPI_TASK_TYPE_FREE);
    result = PMPI_Type_free(datatype);
    ITT_END(MPI_TASK_TYPE_FREE);
    return result;
}
