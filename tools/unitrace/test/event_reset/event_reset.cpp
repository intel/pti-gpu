#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <level_zero/ze_api.h>
#include <iostream>
#ifdef __linux__
    #include <unistd.h>
#else
    #include <windows.h>
#endif

static bool enumAndPickDevice(ze_device_handle_t &hDev, ze_driver_handle_t &hDrv)
{
    uint32_t ndrivers = 0;
    ze_result_t rc = zeDriverGet(&ndrivers, nullptr);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeDriverGet with null rc=%d\n", rc);
        return false;
    }

    printf("ndrivers = %d\n", ndrivers);

    std::vector<ze_driver_handle_t> drivers(ndrivers);
    rc = zeDriverGet(&ndrivers, &drivers[0]);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeDriverGet rc=%d\n", rc);
        return false;
    }

    ze_driver_handle_t driver = drivers[0];

    uint32_t ndevices = 0;
    rc = zeDeviceGet(driver, &ndevices, nullptr);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeDeviceGet with null rc=%d\n", rc);
        return false;
    }

    printf("ndevices = %d\n", ndevices);

    std::vector<ze_device_handle_t> devices(ndevices);
    rc = zeDeviceGet(driver, &ndevices, &devices[0]);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeDeviceGet rc=%d\n", rc);
        return false;
    }

    hDrv = driver;
    hDev = devices[0];
    return true;
}

bool findQueueOrdinal(ze_device_handle_t hDev, uint32_t flag_mask, uint32_t &ordinal)
{
    uint32_t qCount = 0;
    ze_result_t rc = zeDeviceGetCommandQueueGroupProperties(hDev, &qCount, nullptr);
    if (rc != ZE_RESULT_SUCCESS)
        return false;

    std::vector<ze_command_queue_group_properties_t> q_props(qCount);
    rc = zeDeviceGetCommandQueueGroupProperties(hDev, &qCount, &q_props[0]);
    if (rc != ZE_RESULT_SUCCESS)
        return false;

    for (ordinal = 0; ordinal < qCount; ordinal++)
        if ((q_props[ordinal].flags & flag_mask) == flag_mask && q_props[ordinal].numQueues) {
            printf("ordinal=%u numQueues=%u\n", ordinal, q_props[ordinal].numQueues);
            return true;
        }

    return false;
}

int execute_immediate_cl()
{
    zeInit(ZE_INIT_FLAG_GPU_ONLY);

    // --- Get driver, device, context ---
    ze_driver_handle_t driver;
    uint32_t driverCount = 1;
    zeDriverGet(&driverCount, &driver);

    ze_device_handle_t device;
    uint32_t deviceCount = 1;
    zeDeviceGet(driver, &deviceCount, &device);

    uint32_t compQ_ordinal = 0;
    if (!findQueueOrdinal(device, ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY|ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE, compQ_ordinal)) {
        printf("Failed to find device compute queue\n");
        return -1;
    }

    ze_context_handle_t context;
    ze_context_desc_t contextDesc = { ZE_STRUCTURE_TYPE_CONTEXT_DESC };
    zeContextCreate(driver, &contextDesc, &context);

    // --- Create event pool and eventX ---
    ze_event_pool_handle_t eventPool;
    ze_event_pool_desc_t poolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // visible to host
        (uint32_t)(4)
    };
    zeEventPoolCreate(context, &poolDesc, 1, &device, &eventPool);

    ze_event_handle_t eventX;
    ze_event_desc_t eventXDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0, // index
        ZE_EVENT_SCOPE_FLAG_DEVICE, // signal scope
        ZE_EVENT_SCOPE_FLAG_HOST   // wait scope
    };
    zeEventCreate(eventPool, &eventXDesc, &eventX);

    ze_event_handle_t eventXReset;
    ze_event_desc_t eventXResetDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        1, // index
        ZE_EVENT_SCOPE_FLAG_DEVICE, // signal scope
        ZE_EVENT_SCOPE_FLAG_HOST   // wait scope
    };
    zeEventCreate(eventPool, &eventXResetDesc, &eventXReset);

    ze_event_handle_t eventY;
    ze_event_desc_t eventYDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        2, // index
        ZE_EVENT_SCOPE_FLAG_DEVICE, // signal scope
        ZE_EVENT_SCOPE_FLAG_HOST   // wait scope
    };
    zeEventCreate(eventPool, &eventYDesc, &eventY);

    ze_event_handle_t eventZ;
    ze_event_desc_t eventZDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        3, // index
        ZE_EVENT_SCOPE_FLAG_DEVICE, // signal scope
        ZE_EVENT_SCOPE_FLAG_HOST   // wait scope
    };
    zeEventCreate(eventPool, &eventZDesc, &eventZ);

    // --- Create immediate command list ---
    ze_command_queue_desc_t queueDesc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        nullptr,
        compQ_ordinal, 0, 0,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL
    };


    ze_command_list_handle_t immCmdList;
    zeCommandListCreateImmediate(context, device, &queueDesc, &immCmdList);

    ze_event_handle_t kernel_event;

    // --- Append a barrier that signals eventZ ---
    zeCommandListAppendBarrier(immCmdList, eventZ, 0, nullptr);
    printf("Barrier executed -> eventZ signaled by device\n");

    // --- Append a barrier that signals eventX ---
    zeCommandListAppendBarrier(immCmdList, eventX, 0, nullptr);
    printf("Barrier executed -> eventX signaled by device\n");

    // --- Append device-side reset of eventX ---
    zeCommandListAppendEventReset(immCmdList, eventX);
    printf("Device reset of eventX appended\n");

    zeCommandListAppendBarrier(immCmdList, eventXReset, 0, nullptr); // signals eventDone when reset finished
    printf("append barrier for event reset\n");

    // Host waits for eventDone instead of eventX , this make sure that eventX reset was done on device 
    zeEventHostSynchronize(eventXReset, UINT64_MAX);
    printf("host sync for eventX reset event barrier\n");
    
    ze_result_t status = zeEventQueryStatus(eventX);
    printf("query reset eventX status\n");
    int max_check = 10;
    int check = 0;
    while (status == ZE_RESULT_SUCCESS && (check++ < max_check)) {
        #ifdef __linux__
        sleep(100);
        #else
        Sleep(100);
        #endif
        printf("check %d failed", check);
        status = zeEventQueryStatus(eventX);
    }

    if (status == ZE_RESULT_NOT_READY)
        printf("EventX successfully reset on device\n");
    else
        printf("EventX still signaled (reset failed)\n");
     
    status = zeEventDestroy(eventX);
    if (status == ZE_RESULT_SUCCESS)
        printf("eventX destroyed successfully\n");
    else
        printf("eventX fail to destroy\n");

    zeCommandListAppendBarrier(immCmdList, eventY, 0, nullptr);
    //unitrace should crash here since eventX was destroyed
    printf("Host sync after append barrier for eventY , unitrace should crash here\n");
    zeEventHostSynchronize(eventY, UINT64_MAX);

    zeEventDestroy(eventXReset);
    zeEventDestroy(eventY);
    zeEventDestroy(eventZ);    
    zeEventPoolDestroy(eventPool);
    zeCommandListDestroy(immCmdList);
    zeContextDestroy(context);

    printf("immediate cl execution and cleanup ended\n");
    return 0;
}

int execute_non_immediate_cl_single(ze_command_list_flags_t cl_flags, bool use_fence = false, bool use_barrier = false)
{
    ze_result_t rc = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeInit rc=%d\n", rc);
        return -1;
    }
    ze_device_handle_t hDev;
    ze_driver_handle_t hDrv;
    if (!enumAndPickDevice(hDev, hDrv))
        return -1;

    uint32_t compQ_ordinal = 0;
    if (!findQueueOrdinal(hDev, ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY|ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE, compQ_ordinal)) {
        printf("Failed to find device compute queue\n");
        return -1;
    }

    ze_context_desc_t ctx_desc = {
        .stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC,
        .pNext = nullptr,
        .flags = 0
    };

    ze_context_handle_t hCtx;
    rc = zeContextCreate(hDrv, &ctx_desc, &hCtx);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeContextCreate rc=%d\n", rc);
        return -1;
    }

    ze_event_pool_desc_t pool_desc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext = nullptr,
        .flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        .count = (use_barrier && !use_fence) ? (uint32_t)2 : (uint32_t)1
    };

    ze_event_pool_handle_t evPool;
    rc = zeEventPoolCreate(hCtx, &pool_desc, 1, &hDev, &evPool);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeEventPoolCreate rc=%d\n", rc);
        return -1;
    }

    ze_command_queue_desc_t q_desc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext = nullptr,
        .ordinal = compQ_ordinal,
        .index = 0,
        .flags = 0,
        .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL
    };

    ze_command_queue_handle_t q;
    rc = zeCommandQueueCreate(hCtx, hDev, &q_desc, &q);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeCommandQueueCreate rc=%d\n", rc);
        return -1;
    }

    ze_command_list_desc_t cl_desc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
        .pNext = nullptr,
        .commandQueueGroupOrdinal = compQ_ordinal,
        .flags = cl_flags
    };
    ze_command_list_handle_t cl;    
    rc = zeCommandListCreate(hCtx, hDev, &cl_desc, &cl);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeCommandListCreate rc=%d\n", rc);
        return -1;
    }

    ze_event_handle_t eventX;
    ze_event_desc_t eventXDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0, // index
        ZE_EVENT_SCOPE_FLAG_DEVICE, // signal scope
        ZE_EVENT_SCOPE_FLAG_HOST   // wait scope
    };
    zeEventCreate(evPool, &eventXDesc, &eventX);

    ze_event_handle_t completionEvent;
    if (use_barrier && !use_fence) {
        ze_event_desc_t eventDesc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            1, // index
            ZE_EVENT_SCOPE_FLAG_DEVICE,
            ZE_EVENT_SCOPE_FLAG_HOST
        };
        zeEventCreate(evPool, &eventDesc, &completionEvent);
    }

    zeCommandListAppendBarrier(cl, eventX, 0, nullptr);
    printf("Append barrier to signal eventX\n");

    // --- Append device-side reset of eventX ---
    zeCommandListAppendEventReset(cl, eventX);
    printf("Device reset of eventX appended\n");

    if (use_barrier && !use_fence) {
        zeCommandListAppendBarrier(cl, completionEvent, 0, nullptr);
        printf("Append barrier for synchronization\n");
    }

    rc = zeCommandListClose(cl);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeCommandListClose rc=%d\n", rc);
        return -1;
    }

    ze_fence_handle_t fence;
    if (use_fence && !use_barrier) {
        const ze_fence_desc_t f_desc = {
        .stype = ZE_STRUCTURE_TYPE_FENCE_DESC,
        .pNext = nullptr,
        .flags = 0
        };
        printf("Using fence for synchronization\n");
        rc = zeFenceCreate(q, &f_desc, &fence);
        if (rc != ZE_RESULT_SUCCESS) {
            printf("zeFenceCreate rc=%d\n", rc);
            return -1;
        }
    }

    struct timespec exec_time, prev_exec_time;
    uint64_t prev_min_start = UINT64_MAX;

    rc = zeCommandQueueExecuteCommandLists(q, 1, &cl, use_fence? fence : nullptr);
    assert(rc == ZE_RESULT_SUCCESS);

    //unitrace should crash here since eventX was destroyed
    if (use_fence && !use_barrier) {
        printf("Fence Host sync, unitrace should crash here\n");
        rc = zeFenceHostSynchronize(fence, UINT64_MAX);
        assert(rc == ZE_RESULT_SUCCESS);
    }
    if (use_barrier && !use_fence) {
        printf("Host sync for completion event, unitrace should crash here\n");
        rc = zeEventHostSynchronize(completionEvent, UINT64_MAX);
        assert(rc == ZE_RESULT_SUCCESS);
    }
    if (!use_barrier && !use_fence) {
        printf("Queue Host sync, unitrace should crash here\n");
        rc = zeCommandQueueSynchronize(q, UINT64_MAX);
        assert(rc == ZE_RESULT_SUCCESS);
    }

    printf("Host reset eventX\n");
    zeEventHostReset(eventX);
    if (use_barrier && !use_fence) {
        printf("Host reset completionEvent\n");
        zeEventHostReset(completionEvent);
    }
    printf("Device destroy eventX\n");
    zeEventDestroy(eventX);
    if (use_barrier && !use_fence) {
        printf("Device destroy completionEvent\n");
        zeEventDestroy(completionEvent);
    }
    zeEventPoolDestroy(evPool);

    if (use_fence && !use_barrier) {    
        zeFenceDestroy(fence);
    }
    zeCommandListDestroy(cl);
    zeCommandQueueDestroy(q);
    return 0;
}

void print_help_message(char *name)
{
    printf("Usage: %s [-im | -1nicl | -2nicl | -h | -i | -r | -f | -b]\n", name);
    printf("   -1nicl run non-immediate cl in single command queue\n");
    printf("   -2nicl run two concurrent cl with event dependency\n");
    printf("   -im for immediate mode\n");
    printf("   -i for in-order command list [apply only for non-immediate]\n");
    printf("   -r for relaxed-ordering command list [apply only for non-immediate]\n");
    printf("   -f use fence for synchronization [apply only for non-immediate] default is queue sync\n");
    printf("   -b use barrier between launches [apply only for non-immediate] default is queue sync\n");
    printf("   -h print this help message\n");
}

int main(int argc, char *argv[])
{
    ze_command_list_flags_t cl_flags = 0;
    bool use_fence = false;
    bool use_barrier = false;
    bool non_immediate_cl_single = false;
    bool non_immediate_cl_concurrent = false;
    bool execute_immediate = false;
    if (argc == 1) {
        print_help_message(argv[0]);
        return -1;
    }

    for (uint32_t i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) {
            std::cout << "Using in-order command list\n";
            cl_flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
        }
        else if (!strcmp(argv[i], "-r")) {
            std::cout << "Using relaxed ordering for command list\n";
            cl_flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING;
        }
        else if (!strcmp(argv[i], "-f")) {
            std::cout << "Using fence for synchronization\n";
            use_fence = true;
        }
        else if (!strcmp(argv[i], "-b")) {
            std::cout << "Using barrier for synchronization\n";
            use_barrier = true;
        }
        else if (!strcmp(argv[i], "-1nicl")) {
            non_immediate_cl_single = true;
        }
        else if (!strcmp(argv[i], "-2nicl")) {
            non_immediate_cl_concurrent = true;
        }
        else if (!strcmp(argv[i], "-im")) {
             return execute_immediate_cl();
        }
        else if (!strcmp(argv[i], "-h")) {
            print_help_message(argv[0]);
            return -1;
        }
    }

    if (non_immediate_cl_single) {
        std::cout << "Executing non-immediate command list in single queue mode\n";
        if (!use_fence && !use_barrier) {
            std::cout << "Defaulting to queue synchronization\n";
        }
        return execute_non_immediate_cl_single(cl_flags, use_fence, use_barrier);
    }
}
