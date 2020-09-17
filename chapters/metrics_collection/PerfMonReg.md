# Metrics Collection with Performance Monitoring (PM) Register
## Overview
Performance monitoring (PM) register is a special register inside Intel(R) Processor Graphics execution unit (EU) that can be set to collect some particular hardware events (one event at a time).

To set collection for an event, one have to define three register fields:
- Event ID - which event to collect;
- Coarse Filter - filter by shader type and EU raws;
- Fine Filter - filter by instruction type and EUs inside a raw.

Exact event list and filter options can be found in Programmer's Reference Manual for Intel(R) Processor Graphics, [Volume 14: Observability](https://01.org/sites/default/files/documentation/intel-gfx-prm-osrc-kbl-vol14-observability.pdf).

**Supported OS**:
- Linux

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- *i915_drm.h* and *xf86drm.h* from [libdrm](https://01.org/linuxgraphics/community/libdrm) to **set**
- Graphics Technology Pin (GT Pin)  headers from the [release package](../../gtpin) to **read**

**Needed Libraries**:
- [libdrm](https://01.org/linuxgraphics/community/libdrm) to **set**
- Graphics Technology Pin (GT Pin) libraries from the [release package](../../gtpin) to **read**

## How To Use
### Event Set
To tune PM register to collect a particular event one may use Direct Rendering Manager (DRM) interface to deal with Intel(R) Processor Graphics driver.

First of all one have to open DRM interface for i915 driver:
```cpp
int fd = drmOpenWithType("i915", NULL, DRM_NODE_RENDER);
if (fd < 0) {
  fd = drmOpenWithType("i915", NULL, DRM_NODE_PRIMARY);
}
assert(fd >= 0);
```
Second step is to prepare PM register configuration:
```cpp
#define PERF_GUID_LENGTH 37

// PM register is in a group of flexible registers
std::vector<unsigned> flex_regs(2);
flex_regs[0] = PERF_REG_OFFSET;
flex_regs[1] = reg_value; // combined filters + event_id value

std::string guid = GetGuid(flex_regs);

struct drm_i915_perf_oa_config param = {0,};
assert(sizeof(param.uuid) == PERF_GUID_LENGTH - 1);
assert(flex_regs.size() % 2 == 0);

memcpy(param.uuid, guid.c_str(), sizeof(param.uuid));

// Register values
param.boolean_regs_ptr = 0;
param.mux_regs_ptr = 0;
param.flex_regs_ptr = reinterpret_cast<uint64_t>(flex_regs.data());

// Number of register values
param.n_boolean_regs = 0;
param.n_mux_regs = 0;
param.n_flex_regs = static_cast<uint32_t>(flex_regs.size() / 2);

config_id = SendIoctl(fd, DRM_IOCTL_I915_PERF_ADD_CONFIG, &param);
```
The only unobvious field here is GUID, it should be formed as following:
```cpp
std::string GetGuid(const std::vector<unsigned>& regs) {
  std::string regs_string;
  for (size_t i = 0; i < regs.size(); ++i) {
    regs_string += regs[i];
  }

  char guid[PERF_GUID_LENGTH] = { 0 };
  std::hash<std::string> hash;
  snprintf(guid, sizeof(guid), "%08x-%04x-%04x-%04x-%012x", 0, 0, 0, 0,
    static_cast<uint32_t>(hash(regs_string)));
  return guid;
}
```
Finally one have to use received `config_id` to actually enable event collection:
```cpp
uint64_t properties[] = {
  DRM_I915_PERF_PROP_SAMPLE_OA, 1,
  DRM_I915_PERF_PROP_OA_METRICS_SET, static_cast<uint64_t>(config_id),
  DRM_I915_PERF_PROP_OA_FORMAT, I915_OA_FORMAT_A32u40_A4u32_B8_C8 };

struct drm_i915_perf_open_param param = { 0, };
param.flags = 0;
param.flags |= I915_PERF_FLAG_FD_CLOEXEC;
param.flags |= I915_PERF_FLAG_FD_NONBLOCK;
param.properties_ptr = reinterpret_cast<uint64_t>(properties);
param.num_properties = sizeof(properties) / (2 * sizeof(uint64_t));

int stream_fd = SendIoctl(fd, DRM_IOCTL_I915_PERF_OPEN, &param);
```
Starting from this moment PM register will be incremented every cycle target event happened.

To stop event collection and release stream configuration one have to close stream descriptor:
```cpp
drmClose(stream_fd);
drmClose(fd);
```
### Event Read
Since PM register is accessible only from GPU kernel, it's impossible to read it directly from host side. One of the possible ways to read PM results is to use a special Graphics Technology Pin (GT Pin) probe while binary instrumentation called `perfmon`.

Refer to Graphics Technology Pin (GT Pin) [chapter](../binary_instrumentation/GTPin.md) to learn more on binary instrumentation techniques.

## Usage Details
- refer to Programmer's Reference Manual for Intel(R) Processor Graphics, [Volume 14: Observability](https://01.org/sites/default/files/documentation/intel-gfx-prm-osrc-kbl-vol14-observability.pdf) to learn more on performance monitoring (PM) register
- refer to Graphics Technology Pin (GT Pin) Binary Instrumentation [chapter](../binary_instrumentation/GTPin.md) to learn more on the ways of reading PM events

## Samples
- [GPU PerfMon Set](../../samples/gpu_perfmon_set)
- [GPU PerfMon Read](../../samples/gpu_perfmon_read)