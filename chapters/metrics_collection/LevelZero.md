# Metrics Collectoin with oneAPI Level Zero (Level Zero) Metric API
## Overview
oneAPI Level Zero runtime provides a set of API to collect hardware metrics for Intel(R) Processor Graphics.

Each **metric** represents one aspect of GPU performance behaviour and has a unique name, e.g. "EuActive" metric shows the percentage of time GPU execution units were active, "L3Misses" gives the number of L3 cache misses, and "GtiReadThroughput" collects an average throughput while reading data through GTI.

To learn more on Intel(R) Processor Graphics Architecture one may refer to [compute architecture manuals](https://software.intel.com/en-us/articles/intel-graphics-developers-guides).

There are two types of metrics - one should be used for continuous collection and another for query-based.

Different metrics are combined into **metric groups**. Metric group is a unit of collection, so one can collect only a group of metrics instead of each metric separately.

Metric groups in their turn belongs to **metric domains**. Metric groups from different domains can be collected simultaneously, while only a single group from a domain can be enabled at a time.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+
- Intel(R) Processor Graphics KBL+ (for Stream Markers)

**Needed Headers**:
- [zet_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/zet_api.h)

**Needed Libraries**:
- oneAPI Level Zero (Level Zero) [libraries](https://github.com/intel/compute-runtime)

## How To Use

### Enumeration
To enumerate metrics available on the system one have to iterate over all metric groups and next over all separate metrics inside each group.

The first step is to initialize Level Zero core and tools API:
```cpp
ze_result_t status = ZE_RESULT_SUCCESS;
status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
assert(status == ZE_RESULT_SUCCESS);
```
Every group and seprate metric contain a list of properties that helps to determine their purpose (like name or description) as well as to grab some additional information that should be used while collecton (e.g. metric type or domain ID).
```cpp
uint32_t group_count = 0;
status = zetMetricGroupGet(device, &group_count, nullptr);
assert(status == ZE_RESULT_SUCCESS);

zet_metric_group_handle_t* group_list =
  new zet_metric_group_handle_t[group_count];
status = zetMetricGroupGet(device, &group_count, group_list);
assert(status == ZE_RESULT_SUCCESS);

for (uint32_t gid = 0; gid < group_count; ++gid) {
  zet_metric_group_properties_t group_props = {};
  group_props.version = ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT;
  status = zetMetricGroupGetProperties(group_list[gid], &group_props);
  assert(status == ZE_RESULT_SUCCESS);

  zet_metric_handle_t* metric_list =
    new zet_metric_handle_t[group_props.metric_count];
  uint32_t metric_count = group_props.metric_count;
  status = zetMetricGet(group_list[gid], &metric_count, metric_list);
  assert(status == ZE_RESULT_SUCCESS);

  std::cout << group_props.name << " (" << group_props.description <<
    ")" << std::endl;

  for (uint32_t mid = 0; mid < metric_count; ++mid) {
    zet_metric_properties_t metric_props = {};
    metric_props.version = ZET_METRIC_PROPERTIES_VERSION_CURRENT;
    status = zetMetricGetProperties(metric_list[mid], &metric_props);
    assert(status == ZE_RESULT_SUCCESS);

    std::cout << metric_props.name << " (" << metric_props.description <<
      ")" << std::endl;
  }
}
```

### Continuous Collection
Process of metrics collection in continuous mode assumes that there is an infinite loop in a seprate thread, where one asks for collected samples periodically, read the data for a chunk of samples and store them into some memory or file (one sample contains all the metics from a metric group).

First one should choose a metric group to collect, that can be done e.g. while enumeration phase (see above). To activate it, one should call `zetContextActivateMetricGroups` function that will set up the hardware:
```cpp
status = zetContextActivateMetricGroups(context, device, 1, &group);
assert(status == ZE_RESULT_SUCCESS);
```
To be able to wait for metric reports one need to make an event within an event pool:
```cpp
ze_event_pool_desc_t event_pool_desc = {
    ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0,
    1}; // event pool size
ze_event_pool_handle_t event_pool = nullptr;
status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
assert(status == ZE_RESULT_SUCCESS);

ze_event_desc_t event_desc = {
    ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0 /* slot id in pool */,
    ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
ze_event_handle_t event = nullptr;
status = zeEventCreate(event_pool, &event_desc, &event);
assert(status == ZE_RESULT_SUCCESS);
```
Metric tracer object performs the management of metrics collection process. Before its creation one should set target sampling interval (time on GPU in nanoseconds between two reports) and number of reports to collect in one chunk:
```cpp
zet_metric_streamer_desc_t metric_streamer_desc = {
    ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC,
    nullptr,
    32768, /* reports to collect before notify */
    100000 /* sampling period in nanoseconds */};
zet_metric_streamer_handle_t metric_streamer = nullptr;
status = zetMetricStreamerOpen(context, device, group, &metric_streamer_desc,
                                event, &metric_streamer);
assert(status == ZE_RESULT_SUCCESS);
```
Sampling interval shouldn't be too small - it's recommended to make it at least 100 microseconds. In general, smaller interval leads to more data and higher overhead.

To grab collected data, one should make a loop within a separate thread and periodically check for the samples. Function `zeEventHostSynchronize` will be completed either if target number of reports are collected or if it waits more than the set delay:
```cpp
std::vector<uint8_t> buffer; // temporary buffer
while (true) {
  status = zeEventHostSynchronize(
      event, 50000000 /* wait delay in nanoseconds */);
  assert(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);

  size_t data_size = 0;
  status = zetMetricStreamerReadData(metric_streamer, UINT32_MAX
                                     &data_size, nullptr);
  assert(status == ZE_RESULT_SUCCESS);

  if (data_size > 0) {
    buffer.resize(data_size);

    status = zetMetricStreamerReadData(metric_streamer, UINT32_MAX,
                                       &data_size, buffer.data());
    assert(status == ZE_RESULT_SUCCESS);

    // Store buffer into the memory or file
  }
}
```
To finalize the collection process one need to close metric tracer, release all the objects and deactivate metric group:
```cpp
status = zetMetricStreamerClose(metric_streamer);
assert(status == ZE_RESULT_SUCCESS);

status = zeEventDestroy(event);
assert(status == ZE_RESULT_SUCCESS);
status = zeEventPoolDestroy(event_pool);
assert(status == ZE_RESULT_SUCCESS);

status = zetContextActivateMetricGroups(context, device, 0, nullptr);
assert(status == ZE_RESULT_SUCCESS);
```

### Stream Markers
***NOTE: not working currently due to Linux kernel issue.***

Level Zero Metrics API provides a special mechanism to mark any subset of GPU tasks and measure its execution time - e.g. one may get execution time for a specific kernel.

To do this one should enable continuous mode of metrics collection for the metric group that contains `RenderStreamMarker` metric, e.g. `RenderBasic` or `ComputeBasic` (works for KBL+).

From API perspective stream marker looks like an additional type of command list task one should append before and after target GPU activity. Each marker should provide its own unique identifier:
```cpp
ze_result_t status = ZE_RESULT_SUCCESS;
status = zetCommandListAppendMetricStreamerMarker(
    command_list, metric_streamer, 0 /* start marker identifier */);
assert(status == ZE_RESULT_SUCCESS);

/* Append measured GPU tasks to command list */

status = zetCommandListAppendMetricStreamerMarker(
    command_list, metric_streamer, 1 /* end marker identifier */);
assert(status == ZE_RESULT_SUCCESS);
```
After the metrics will be collected and calculated, one can find previously set marker values in `RenderStreamMarker` metric for two reports - the first one will be collected before the target activity, and the second one - after.

Each of these reports contains `QueryBeginTime` value that shows the time in nanosends when the report measurement is started. So to get activity execution time, one should compute the difference between these two values.

### Query-Based Collection
An alternate way to collect hardware metrics is query-based mode that allows one to map collected results directly to device activity, e.g.  kernel execution or memory transfer.

Similarly to the continuous mode, the first step should be to determine a group of metrics to collect and set up the hardware:
```cpp
status = zetContextActivateMetricGroups(context, device, 1, &group);
assert(status == ZE_RESULT_SUCCESS);
```
Next one have to create metric query pool and event pool to deal with particular metric queries:
```cpp
zet_metric_query_pool_desc_t metric_query_pool_desc = {
    ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC, nullptr,
    ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE,
    MAX_KERNEL_COUNT /* pool size */ };
zet_metric_query_pool_handle_t metric_query_pool = nullptr;
status = zetMetricQueryPoolCreate(
    context, device, group, &metric_query_pool_desc, &metric_query_pool);
assert(status == ZE_RESULT_SUCCESS);

ze_event_pool_desc_t event_pool_desc = {
    ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0,
    MAX_KERNEL_COUNT /* pool size */ };
ze_event_pool_handle_t event_pool = nullptr;
status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
assert(status == ZE_RESULT_SUCCESS);
```
This time one may have to deal with multiple metric queries at a time (e.g. if multiple kernels are submitted to execution simultaniously), so the size for both pools may be greater than one.

The process of metric query submission looks like this:
```cpp
zet_metric_query_handle_t metric_query = nullptr;
status = zetMetricQueryCreate(metric_query_pool, slot_id, &metric_query);
assert(status == ZE_RESULT_SUCCESS);

status = zetCommandListAppendMetricQueryBegin(command_list, metric_query);
assert(status == ZE_RESULT_SUCCESS);

// Append any command to command list one wants to profile
// e.g. call zeCommandListAppendLaunchKernel(...)

ze_event_desc_t event_desc = {
    ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, slot_id,
    ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
ze_event_handle_t event = nullptr;
status = zeEventCreate(event_pool, &event_desc, &event);
assert(status == ZE_RESULT_SUCCESS);

status = zetCommandListAppendMetricQueryEnd(command_list, metric_query, event, 0, nullptr);
assert(status == ZE_RESULT_SUCCESS);
```
After the submission one may wait for target query completion based on an appropriate event:
```cpp
status = zeEventHostSynchronize(event, UINT32_MAX);
assert(status == ZE_RESULT_SUCCESS);
```
Function `zetMetricQueryGetData` allows to read data from query. It will be a single report in raw format:
```cpp
size_t raw_size = 0;
status = zetMetricQueryGetData(metric_query, &raw_size, nullptr);
assert(status == ZE_RESULT_SUCCESS);

std::vector<uint8_t> raw_data(raw_size);
status = zetMetricQueryGetData(metric_query, &raw_size, raw_data.data());
assert(status == ZE_RESULT_SUCCESS);
```
After collection is done, it's worth to release all the memory and deactivate the metric group:
```cpp
status = zeEventDestroy(event);
assert(status == ZE_RESULT_SUCCESS);

status = zetMetricQueryDestroy(metric_query);
assert(status == ZE_RESULT_SUCCESS);

status = zeEventPoolDestroy(event_pool);
assert(status == ZE_RESULT_SUCCESS);

status = zetMetricQueryPoolDestroy(metric_query_pool);
assert(status == ZE_RESULT_SUCCESS);

status = zetContextActivateMetricGroups(context, device, 0, nullptr);
assert(status == ZE_RESULT_SUCCESS);
```
Each calculated quety metric report contains the field `GpuTime` that provides the time spent on GPU in nanoseconds - so it can be used to measure kernel/transfer execution time on the device.

### Calculation
Metric reports collected at the previous stages are in raw format and should be post-calculated to become user-readable. To perform calculations one should call `zetMetricGroupCalculateMetricValues` function for target metric group.

Each calculated report contains an array of metric values. An order of values directly corresponds to the order of metrics one can learn while enumeration.

```cpp
std::vector<uint8_t> data; // raw metric reports
uint32_t value_count = 0;
status = zetMetricGroupCalculateMetricValues(
    group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, data.size(),
    (uint8_t*)data.data(), &value_count, nullptr);
assert(status == ZE_RESULT_SUCCESS);

zet_typed_value_t* value_list = new zet_typed_value_t[value_count];
status = zetMetricGroupCalculateMetricValues(
    group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, data.size(),
    (uint8_t*)data.data(), &value_count, value_list);
assert(status == ZE_RESULT_SUCCESS);
```

### Time Correlation
To map metric report to some particular device activity (e.g. kernel execution) one need to correlate report timestamp (metric with name `QueryBeginTime` inside a report) to kernel `global` timestamps retrieved with the help of [device activity tracing](../device_activity_tracing/LevelZero.md).

The difference between these two timestamps is that kernel time is in GPU clocks, and `QueryBeginTime` is in nanoseconds. To convert clocks to nanoseconds one need to know GPU timer frequency and use the following formula:
```
NS_IN_SEC = 1000000000
gpuTimestampNs = gpuTimestampClocks * NS_IN_SEC / gpuTimerFrequency
```
Starting from version 1.1, Level Zero provides this value as `timerResolution` field of `ze_device_properties_t` structure in cycles per second. Also it can be retrieved with the help of Intel(R) Metrics Discovery Application Programming Interface as part of device information as `GpuTimestampFrequency` symbol (look into "Device Information" section from [here](./MetricsDiscoveryAPI.md) for details).

## Build and Run
To make metrics collection work one need to link the application with Level Zero ICD library (e.g. `libze_loader.so`) and run it as following:
```
ZET_ENABLE_METRICS=1 ./<application>
```

## Usage Details
- refer to oneAPI Level Zero Metrics [documentation](https://spec.oneapi.com/level-zero/latest/tools/PROG.html#metrics) to learn more

## Samples
- [Level Zero Metric Info](../../samples/ze_metric_info)
- [Level Zero Metric Query](../../samples/ze_metric_query)
- [Level Zero Metric Streamer](../../samples/ze_metric_streamer)

## Tools
- [GPU Metrics Collection Tool for Data Parallel C++ (DPC++)](../../tools/oneprof)