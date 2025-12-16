#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <level_zero/ze_api.h>
#include "empty_kernel.h"

#ifdef _WIN32
#include <windows.h>
struct wintimespec {
    time_t tv_sec;  // seconds
    long   tv_nsec; // nanoseconds
};

static int clock_getwintime(struct wintimespec* ts) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    ts->tv_sec = count.QuadPart / freq.QuadPart;
    ts->tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1e9 / freq.QuadPart);
    return 0;
}
#endif

static bool enumAndPickDevice(ze_device_handle_t &hDev, ze_driver_handle_t &hDrv)
{
    uint32_t ndrivers = 0;
    ze_result_t rc = zeDriverGet(&ndrivers, nullptr);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeDriverGet with null rc=%d\n", rc);
        return false;
    }

    printf("ndrivers = %d\n", ndrivers);

    std::vector<ze_driver_handle_t> drivers(ndrivers);
    rc = zeDriverGet(&ndrivers, &drivers[0]);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeDriverGet rc=%d\n", rc);
        return false;
    }

    ze_driver_handle_t driver = drivers[0];

    uint32_t ndevices = 0;
    rc = zeDeviceGet(driver, &ndevices, nullptr);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeDeviceGet with null rc=%d\n", rc);
        return false;
    }

    printf("ndevices = %d\n", ndevices);

    std::vector<ze_device_handle_t> devices(ndevices);
    rc = zeDeviceGet(driver, &ndevices, &devices[0]);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeDeviceGet rc=%d\n", rc);
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

#ifdef _WIN32
static long timespec_diff_ms(struct wintimespec start, struct wintimespec end) {
#else
static long timespec_diff_ms(struct timespec start, struct timespec end) {
#endif
    long seconds_diff = end.tv_sec - start.tv_sec;
    long nanoseconds_diff = end.tv_nsec - start.tv_nsec;

    // Normalize if nanoseconds are negative
    if (nanoseconds_diff < 0) {
        seconds_diff -= 1;
        nanoseconds_diff += 1000000000;
    }

    // Convert total time to milliseconds
    return seconds_diff * 1000 + nanoseconds_diff / 1000000;
}


int main(int argc, char *argv[])
{
    size_t buf_size = 4096;
    uint32_t n_launches = 10;
    uint32_t cl_flags = 0;
    uint32_t n_submits = 1;
    uint32_t n_submit_interval_ms = 0;
    bool infinite_submit = false;

    for (uint32_t i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i")) {
            cl_flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
        }
        else if (!strcmp(argv[i], "-r")) {
            cl_flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING;
        }
        else if (!strcmp(argv[i], "-n") && i++ < argc) {
            n_launches = std::stoi(argv[i]);
        }
        else if (!strcmp(argv[i], "-s") && i++ < argc) {
            n_submits = std::stoi(argv[i]);
            infinite_submit = (n_submits == 0);
        }
        else if (!strcmp(argv[i], "-t") && i++ < argc) {
            n_submit_interval_ms = std::stoi(argv[i]);
        }
        else if (!strcmp(argv[i], "-h")) {
            printf("Usage: %s [-i | -r] [-n <num_launches>]\n", argv[0]);
            printf("   -i for IN_ORDER command list\n");
            printf("   -r for RELAXED_ORDERING command list\n");
            printf("   -n <kernel count> for number of kernelks in command list\n");
            printf("   -s <submit count> for number of command list submit , 0 for infinite \n");
            printf("   -t <ms> submit interval\n");

            return -1;
        }
    }

    ze_result_t rc = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeInit rc=%d\n", rc);
        return -1;
    }

    ze_device_handle_t hDev;
    ze_driver_handle_t hDrv;
    if (!enumAndPickDevice(hDev, hDrv))
    {
        printf("[ERROR] Failed to pick device\n");
        return -1;
    }

    uint32_t compQ_ordinal = 0;
    if (!findQueueOrdinal(hDev, ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY|ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE, compQ_ordinal)) {
        printf("[ERROR] Failed to find device compute queue\n");
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
        printf("[ERROR] zeContextCreate rc=%d\n", rc);
        return -1;
    }

    ze_kernel_handle_t emptyKern = getEmptyKernel(hDev, hCtx);
    if (!emptyKern) {
        printf("[ERROR] Failed to create empty kernel\n");
        return -1;
    }

    ze_event_pool_desc_t pool_desc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext = nullptr,
        .flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
        .count = n_launches
    };

    ze_event_pool_handle_t evPool;
    rc = zeEventPoolCreate(hCtx, &pool_desc, 1, &hDev, &evPool);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeEventPoolCreate rc=%d\n", rc);
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
        printf("[ERROR] zeCommandQueueCreate rc=%d\n", rc);
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
        printf("[ERROR] zeCommandListCreate rc=%d\n", rc);
        return -1;
    }

    std::vector<ze_event_handle_t> ev(n_launches);
    ze_group_count_t dim = {1, 1, 1};
    for (unsigned i = 0; i < n_launches; i++) {
        ze_event_desc_t ev_desc = {
            .stype = ZE_STRUCTURE_TYPE_EVENT_DESC,
            .pNext = nullptr,
            .index = i,
            .signal = ZE_EVENT_SCOPE_FLAG_DEVICE,
            .wait = ZE_EVENT_SCOPE_FLAG_DEVICE,
        };
        rc = zeEventCreate(evPool, &ev_desc, &ev[i]);
        if (rc != ZE_RESULT_SUCCESS) {
            printf("[ERROR] zeEventCreate rc=%d\n", rc);
            return -1;
        }

        rc = zeCommandListAppendLaunchKernel(cl, emptyKern, &dim, ev[i], 0, nullptr);
        if (rc != ZE_RESULT_SUCCESS) {
            printf("[ERROR] zeCommandListAppendLaunchKernel rc=%d\n", rc);
            return -1;
        }
    }

    rc = zeCommandListClose(cl);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeCommandListClose rc=%d\n", rc);
        return -1;
    }

    const ze_fence_desc_t f_desc = {
        .stype = ZE_STRUCTURE_TYPE_FENCE_DESC,
        .pNext = nullptr,
        .flags = 0
    };

    ze_fence_handle_t fence;
    rc = zeFenceCreate(q, &f_desc, &fence);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("[ERROR] zeFenceCreate rc=%d\n", rc);
        return -1;
    }

#ifdef _WIN32
    struct wintimespec exec_time, prev_exec_time;
#else
    struct timespec exec_time, prev_exec_time;
#endif
    uint64_t prev_min_start = UINT64_MAX;

    int i = 0;
    bool paused = false;
    while (n_submits || infinite_submit) {
#ifdef _WIN32
        clock_getwintime(&exec_time);
#else
        clock_gettime(CLOCK_REALTIME, &exec_time);
#endif
        if (n_submit_interval_ms != 0 && prev_min_start != UINT64_MAX) //not first submit
        {
            long diff = timespec_diff_ms(prev_exec_time, exec_time);

            if (diff < n_submit_interval_ms)
            {
                continue;
            }
        }

        rc = zeCommandQueueExecuteCommandLists(q, 1, &cl, fence);
        assert(rc == ZE_RESULT_SUCCESS);
    
        rc = zeFenceHostSynchronize(fence, UINT64_MAX);
        assert(rc == ZE_RESULT_SUCCESS);
    
        ze_device_properties_t devProp = {
            .stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2,
            .pNext = nullptr
        };
        rc = zeDeviceGetProperties(hDev, &devProp);
        assert(rc == ZE_RESULT_SUCCESS);
    
        printf("device timestamp ticks freq: %ld n_launches=%u n_submits=%u \n", devProp.timerResolution, n_launches, n_submits);
        double ts_to_nano = 1e9 / devProp.timerResolution;
    
        std::vector<ze_kernel_timestamp_result_t> ts(n_launches);
        uint64_t min_start = UINT64_MAX;
        uint64_t max_end = 0;
    
        for (unsigned i = 0; i < n_launches; i++) {
            zeEventQueryKernelTimestamp(ev[i], &ts[i]);
            if (ts[i].global.kernelStart < min_start) min_start = ts[i].global.kernelStart;
            if (ts[i].global.kernelEnd > max_end) max_end = ts[i].global.kernelEnd;
        }
    
        if (cl_flags == ZE_COMMAND_LIST_FLAG_IN_ORDER) {
            for (unsigned i = 0; i < n_launches; i++) {
                uint64_t gap = i < 1 ? 0 : ts[i].global.kernelStart - ts[i-1].global.kernelEnd;
                uint64_t dur = ts[i].global.kernelEnd - ts[i].global.kernelStart;
                printf("%u: start: %ld end: %ld dur: %ld (ticks) gap: %ld (ticks) dur: %lf (ns) gap: %lf (ns)\n", i, ts[i].global.kernelStart, ts[i].global.kernelEnd, dur,
                       gap, (double)dur * ts_to_nano, (double)gap * ts_to_nano);
            }
        }
    
        uint64_t total_dur = max_end - min_start;
        printf("Total device time: %ld (ticks)  %lf ns avg_per_lauch: %lf ns\n", total_dur, (double)total_dur * ts_to_nano, ((double)total_dur * ts_to_nano) / n_launches);

        if (prev_min_start != UINT64_MAX) {
            uint64_t prev_exec_ns = prev_exec_time.tv_sec * 1000000000 + prev_exec_time.tv_nsec;
            uint64_t exec_ns = exec_time.tv_sec * 1000000000 + exec_time.tv_nsec;
            uint64_t wall_dt = exec_ns - prev_exec_ns;
            uint64_t tick_dt = min_start - prev_min_start;
            double tick_freq = ((double)tick_dt * 1e9) / wall_dt;
            printf("Delta time between submits: cpu: %ld ns gpu_ticks: %ld (calc tick freq: %lf)\n", wall_dt, tick_dt, tick_freq);
        }

        n_submits--;
        if (infinite_submit || n_submits) {
            for (unsigned i = 0; i < n_launches; i++)
                zeEventHostReset(ev[i]);
            prev_exec_time = exec_time;
            prev_min_start = min_start;
        }
    }

    zeFenceDestroy(fence);
    zeCommandListDestroy(cl);
    zeCommandQueueDestroy(q);
}
