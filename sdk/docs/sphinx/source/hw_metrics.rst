================================================================
Hardware Metrics Available on Intel(R) Arc(TM) B-Series Graphics
================================================================

The GPU driver provides the metrics in groups, and many metrics are in more than one group. Metrics collected together must all belong to the same group; the **Groups** column below shows which metrics you can combine.

Metrics to Start With
=====================

These metrics are in the ``ComputeBasic`` group, so they can be collected together:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Metric
     - Description
   * - ``XVE_ACTIVE``
     - Percentage of time in which at least one pipe of the Xe Vector Engine (XVE) is active. Higher is better.
   * - ``XVE_STALL``
     - Percentage of time in which threads are loaded on the XVE but no pipe is active. Lower is better.
   * - ``XVE_THREADS_OCCUPANCY_ALL``
     - Percentage of XVE thread slots occupied by threads.
   * - ``GPU_MEMORY_BYTE_READ``, ``GPU_MEMORY_BYTE_WRITE``
     - Number of bytes the kernel read from and wrote to GPU memory.
   * - ``GpuTime``
     - Time elapsed on the GPU during the measurement.
   * - ``AvgGpuCoreFrequencyMHz``
     - Average GPU core frequency during the measurement.
   * - ``GPGPU_DISPATCH``
     - Percentage of time spent dispatching compute threads to the XVEs.
   * - ``L3_READ``, ``L3_WRITE``
     - Number of 64-byte read and write requests to the L3 (device cache).
   * - ``L3_HIT``, ``L3_MISS``
     - Number of L3 accesses that hit and that missed.
   * - ``L3_STALL``
     - Percentage of time in which an L3 bank stalled.

All Metrics
===========

Each metric is listed once, with the driver's description and the groups that have it. "All groups" means every group on this page.

.. The tables are generated from a ptiMetricsGetMetricGroups dump of an Arc B580
   (event-based groups) and the descriptions in intel/metrics-discovery
   docs/metric_info_BMG.tsv. Regenerate them instead of editing by hand.

.. BEGIN GENERATED TABLES

GPU Time and Frequency
----------------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``GpuTime``

       Time elapsed on the GPU during the measurement
     - All groups
   * - ``GpuCoreClocks``

       The total number of GPU core clocks elapsed during the measurement
     - All groups
   * - ``AvgGpuCoreFrequencyMHz``

       Average GPU Core Frequency in the measurement
     - All groups
   * - ``ResultUncertainty``

       Result uncertainty indicator
     - All groups

Vector Engine
-------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``ICACHE_HIT``

       Number of Instruction Cache hits
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``ICACHE_MISS``

       Number of Instruction Cache misses
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_ACTIVE``

       Percentage of time in which at least one pipe is active in XVE
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_ALL``

       Number of execution slots taken by instructions executed by ALL threads on ALU0 pipe
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_ALU1_ALL``

       Number of execution slots taken by instructions executed by ALL threads on ALU1 pipe. Does not include extended math instructions
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_SEND_ALL``

       Number of instruction (GRF or SRF) dispatches executed by ALL threads on SEND Pipe
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_ALU0_CS``

       Number of execution slots taken by instructions executed by CS threads on ALU0 pipe
     - ``RenderBasic``
   * - ``XVE_INST_EXECUTED_ALU0_PS``

       Number of execution slots taken by instructions executed by PS threads on ALU0 pipe
     - ``RenderBasic``
   * - ``XVE_INST_EXECUTED_ALU0_VS``

       Number of execution slots taken by instructions executed by VS threads on ALU0 pipe
     - ``RenderBasic``
   * - ``XVE_INST_EXECUTED_ALU1_CS``

       Number of execution slots taken by instructions executed by CS threads on ALU1 pipe. Does not include extended math instructions
     - ``RenderBasic``
   * - ``XVE_INST_EXECUTED_ALU1_PS``

       Number of execution slots taken by instructions executed by PS threads on ALU1 pipe. Does not include extended math instructions
     - ``RenderBasic``
   * - ``XVE_INST_EXECUTED_ALU1_VS``

       Number of execution slots taken by instructions executed by VS threads on ALU1 pipe. Does not include extended math instructions
     - ``RenderBasic``
   * - ``XVE_SHARED_FUNCTION_ACCESS_HOLD``

       Percentage of time in which XVE requests stalled by Shared Function units
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineStalls``
   * - ``XVE_THREADS_OCCUPANCY_ALL``

       Percentage of thread slots occupied by ALL threads
     - ``RenderBasic``, ``ComputeBasic``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_ISSUED_ALL``

       Number of instructions issued (decoded) to any pipe
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_STALL``

       Percentage of time in which any thread loaded but not even a single pipe is active in XVE
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION``

       Percentage of time taken by instructions executed by ALL threads on ALU0 pipe
     - ``RenderBasic``, ``ComputeBasic``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_ALL_UTILIZATION``

       Percentage of time taken by instructions executed by ALL threads on ALU1 pipe. Does not include extended math instructions
     - ``RenderBasic``, ``ComputeBasic``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU2_ALL``

       Number of execution slots taken by instructions executed in ALU2 pipe
     - ``ComputeBasic``, ``VectorEngineProfile``
   * - ``XVE_MULTIPLE_PIPE_ACTIVE``

       Percentage of time in which at least two pipes are actively executing a Gen ISA instruction among ALU0, ALU1 and ALU2 pipes
     - ``ComputeBasic``, ``VectorEngineProfile``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_PIPE_ALU0_AND_ALU1_ACTIVE``

       Percentage of time in which ALU0 and ALU1 pipes are both actively executing a Gen ISA instruction
     - ``ComputeBasic``, ``VectorEngineProfile``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_PIPE_ALU0_AND_ALU2_ACTIVE``

       Percentage of time in which ALU0 and ALU2 pipes are both actively executing a Gen ISA instruction
     - ``ComputeBasic``, ``VectorEngineProfile``, ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU2_ALL_UTILIZATION``

       Percentage of time taken by instructions executed in ALU2 pipe
     - ``ComputeBasic``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_AHS``

       Number of execution slots taken by instructions executed by RT_AHS threads on ALU0 pipe
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_CHS``

       Number of execution slots taken by instructions executed by RT_CHS threads on ALU0 pipe
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_MS``

       Number of execution slots taken by instructions executed by RT_MS threads on ALU0 pipe
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_AHS``

       Number of execution slots taken by instructions executed by RT_AHS threads on ALU1 pipe. Does not include extended math instructions
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_CHS``

       Number of execution slots taken by instructions executed by RT_CHS threads on ALU1 pipe. Does not include extended math instructions
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_MS``

       Number of execution slots taken by instructions executed by RT_MS threads on ALU1 pipe. Does not include extended math instructions
     - ``RTProfile``
   * - ``XVE_INST_EXECUTED_BARRIER``

       Number of sync.bar and sync.host instructions executed
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_BITCONV``

       Number of execution slots taken by bit manipulation instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_CONTROL_ALL``

       Number of instructions executed on JEU Pipe
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_FP16``

       Number of execution slots taken by FP16 ALU instructions. Does not include extended math instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_FP32``

       Number of execution slots taken by FP32 ALU instructions. Does not include extended math instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_FP64``

       Number of execution slots taken by FP64 ALU instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_FP64_2ND``

       Number of execution slots taken by FP64 ALU instructions executed in the second pipe
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_INT16``

       Number of execution slots taken by INT16 ALU instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_INT32``

       Number of execution slots taken by INT32 ALU instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_INT64``

       Number of execution slots taken by INT64 instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_MATH``

       Number of execution slots taken by extended math instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_NONDIVERGENT``

       Number of non-divergent instructions (not execution slots) issued
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_PREDICATION``

       Number of instructions (not execution slots) issued with predication mask enabled
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_XMX_BF16``

       Number of execution slots taken by BF16 XMX instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_XMX_FP16``

       Number of execution slots taken by FP16 XMX instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_XMX_INT2``

       Number of execution slots taken by INT2 XMX instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_XMX_INT4``

       Number of execution slots taken by INT4 XMX instructions
     - ``VectorEngineProfile``
   * - ``XVE_INST_EXECUTED_XMX_INT8``

       Number of execution slots taken by INT8 XMX instructions
     - ``VectorEngineProfile``
   * - ``XVE_STALL_ALUWR``

       Percentage of time in which XVE stalled, with at least one thread waiting for ALU to write GRF/ACC register. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_BARRIER``

       Percentage of time in which XVE stalled, with at least one thread waiting for Gateway to write Notify register. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_CONTROL``

       Percentage of time in which XVE stalled, with at least one thread waiting for JEU to complete branch instruction. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_INSTFETCH``

       Percentage of time in which XVE stalled, with at least one thread waiting for Instruction Fetch. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_OTHER``

       Percentage of time in which XVE stalled, with at least one thread waiting on any other dependency (Flag/EoT etc). Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_PIPESTALL``

       Percentage of time in which XVE stalled, with at least one thread ready to be scheduled (Grf conf/send holds etc). Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_PS_DEPENDENCY``

       Percentage of time in which XVE stalled, with at least one thread waiting on Pixel Shader dependency
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_SBID``

       Percentage of time in which XVE stalled, with at least one thread waiting for Scoreboard token to be available. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_STALL_SENDWR``

       Percentage of time in which XVE stalled, with at least one thread waiting for SEND message to be dispatched from XVE. Multiple stall reasons can qualify during the same cycle
     - ``VectorEngineStalls``, ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_VS_UTILIZATION``

       Percentage of time taken by instructions executed by VS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU0_MS_UTILIZATION``

       Percentage of time taken by instructions executed by MS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU0_TS_UTILIZATION``

       Percentage of time taken by instructions executed by TS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU0_PS_UTILIZATION``

       Percentage of time taken by instructions executed by PS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_ASYNC_CS_UTILIZATION``

       Percentage of time taken by instructions executed by ASYNC_CS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_CS_UTILIZATION``

       Percentage of time taken by instructions executed by CS threads on ALU0 pipe
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_VS_UTILIZATION``

       Percentage of time taken by instructions executed by VS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU1_MS_UTILIZATION``

       Percentage of time taken by instructions executed by MS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU1_TS_UTILIZATION``

       Percentage of time taken by instructions executed by TS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``
   * - ``XVE_INST_EXECUTED_ALU1_PS_UTILIZATION``

       Percentage of time taken by instructions executed by PS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_ASYNC_CS_UTILIZATION``

       Percentage of time taken by instructions executed by ASYNC_CS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_CS_UTILIZATION``

       Percentage of time taken by instructions executed by CS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRasterizationProfile``, ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_AHS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_AHS threads on ALU0 pipe
     - ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_CHS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_CHS threads on ALU0 pipe
     - ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU0_RT_MS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_MS threads on ALU0 pipe
     - ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_AHS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_AHS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_CHS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_CHS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRaytracingProfile``
   * - ``XVE_INST_EXECUTED_ALU1_RT_MS_UTILIZATION``

       Percentage of time taken by instructions executed by RT_MS threads on ALU1 pipe. Does not include extended math instructions
     - ``XvePipelineRaytracingProfile``

Thread Dispatch
---------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``GPGPU_THREADGROUP_COUNT``

       Number of GPGPU threadgroups dispatched
     - ``RenderBasic``, ``ComputeBasic``, ``DepthProfile``, ``DeviceCacheProfile``, ``MemoryProfile``, ``RenderPipeProfile``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``
   * - ``ASYNC_GPGPU_THREADGROUP_COUNT``

       Number of Async GPGPU threadgroups dispatched
     - ``RenderBasic``, ``ComputeBasic``, ``DepthProfile``, ``DeviceCacheProfile``, ``MemoryProfile``, ``RenderPipeProfile``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``
   * - ``ASYNC_GPGPU_THREAD_EXIT_COUNT``

       Number of Async GPGPU EOT messages received
     - ``ComputeBasic``
   * - ``GPGPU_DISPATCH``

       Percentage of time in which spent dispatching GPGPU threads to XVEs
     - ``ComputeBasic``
   * - ``TASK_THREAD_EXIT_COUNT``

       Number of Task Shader EOT messages received
     - ``RenderPipeProfile``
   * - ``TASK_THREADGROUP_COUNT``

       Number of Task Shader threadgroups dispatched
     - ``RenderPipeProfile``
   * - ``THREAD_DISPATCH_INPUT_AVAILABLE``

       Percentage of time in which Thread Dispatch input is available
     - ``RenderPipeProfile``
   * - ``THREAD_DISPATCH_PS_ACTIVE``

       Percentage of time in which Pixel Shader threads are ready for dispatch
     - ``RenderPipeProfile``
   * - ``THREAD_DISPATCH_QUEUE0_ACTIVE``

       Percentage of time in which non-Pixel Shader threads are ready for dispatch in a particular Xe core
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREAD_DISPATCH_QUEUE0_STALL``

       Percentage of time in which queue 0 is stalled waiting for threads to be available
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREAD_DISPATCH_QUEUE1_ACTIVE``

       Percentage of time in which non-Pixel Shader threads are ready for dispatch in a particular Xe core
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREAD_DISPATCH_QUEUE1_STALL``

       Percentage of time in which queue 1 is stalled waiting for threads to be available
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREADGROUP_DISPATCH_QUEUE0_RESOURCE_STALL``

       Percentage of time in which Thread Spawner queue 0 is stalled waiting for any resource to be available (for example, SLM, Barrier, BTD stack, etc.)
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREADGROUP_DISPATCH_QUEUE1_RESOURCE_STALL``

       Percentage of time in which Thread Spawner queue 1 is stalled waiting for any resource to be available (for example, SLM, Barrier, BTD stack, etc.)
     - ``RenderPipeProfile``, ``VectorEngineStalls``
   * - ``THREADGROUP_DISPATCH_QUEUE0_INPUT_AVAILABLE``

       Percentage of time in which Thread Spawner has input available on queue 0
     - ``VectorEngineStalls``
   * - ``THREADGROUP_DISPATCH_QUEUE1_INPUT_AVAILABLE``

       Percentage of time in which Thread Spawner has input available on queue 1
     - ``VectorEngineStalls``

L1 Cache and Shared Local Memory
--------------------------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``SLM_BANK_CONFLICT_COUNT``

       Number of SLM accesses resulting in a bank conflict
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_HIT``

       Number of Load Store Cache hits
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_ACCESS``

       Number of Load Store Cache accesses
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_BYTE_READ``

       Number of bytes read out of the Load Store Cache, excluding SLM accesses
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_BYTE_WRITE``

       Number of bytes written to the Load Store Cache, excluding SLM accesses
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``SLM_BYTE_READ``

       Number of bytes read from SLM
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``SLM_BYTE_WRITE``

       Number of bytes written to SLM
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_PARTIAL_WRITE_COUNT``

       Number of writes to the Load Store Cache that don't fill a subsector
     - ``ComputeBasic``, ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_INPUT_AVAILABLE``

       Percentage of time in which the Load Store Cache has input available
     - ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_NUMBER_OF_BANK_ACCESS_COUNT``

       Number of Load Store Cache banks accessed in a clock
     - ``MemoryProfile``
   * - ``LOAD_STORE_CACHE_OUTPUT_READY``

       Percentage of time in which the Load Store Cache has output ready
     - ``MemoryProfile``
   * - ``SLM_ACCESS_COUNT``

       Number of SLM accesses
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_ATOMIC_MESSAGE_COUNT``

       Number of atomic operations sent by XVEs to the Load Store Cache
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_FENCE_MESSAGE_COUNT``

       Number of fence messages sent by XVEs to the Load Store Cache
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_READ_MESSAGE_COUNT``

       Number of read messages sent by XVEs to the Load Store Cache
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_REGISTER_REQUEST_COUNT``

       Number of message payload transactions sent by XVEs to the Load Store Cache
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_REGISTER_RESPONSE_COUNT``

       Number of message payload transactions sent from the Load Store Cache back to XVEs
     - ``MemoryProfile``
   * - ``XVE_LOAD_STORE_CACHE_WRITE_MESSAGE_COUNT``

       Number of write messages sent by XVEs to the Load Store Cache
     - ``MemoryProfile``
   * - ``XVE_SLM_ATOMIC_MESSAGE_COUNT``

       Number of SLM atomic operations sent by XVEs
     - ``MemoryProfile``
   * - ``XVE_SLM_FENCE_MESSAGE_COUNT``

       Number of SLM fence operations sent by XVEs
     - ``MemoryProfile``
   * - ``XVE_SLM_READ_MESSAGE_COUNT``

       Number of SLM read messages sent by XVEs
     - ``MemoryProfile``
   * - ``XVE_SLM_WRITE_MESSAGE_COUNT``

       Number of SLM write messages sent by XVEs
     - ``MemoryProfile``
   * - ``RT_LOAD_STORE_CACHE_READ_MESSAGE_COUNT``

       Number of read messages sent from Ray Tracing unit to the Load Store Cache
     - ``RTProfile``
   * - ``RT_LOAD_STORE_CACHE_WRITE_FROM_MESSAGE_COUNT``

       Number of write messages sent from Ray Tracing unit to the Load Store Cache
     - ``RTProfile``

Device Cache (L3)
-----------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``L3_READ``

       Number of Device Cache 64B read requests
     - ``RenderBasic``, ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_WRITE``

       Number of Device Cache 64B write requests
     - ``RenderBasic``, ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_ATOMIC_ACCESS``

       Number of atomic accesses to Device Cache bank
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_HIT``

       Number of Device Cache accesses which hits in the Device Cache cache
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_MISS``

       Number of Device Cache accesses which miss in the Device Cache cache
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_STALL``

       Percentage of time in which Device Cache bank stalled
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``AMFS_L3_ACCESS``

       Number of AMFS access to Device Cache
     - ``DeviceCacheProfile``
   * - ``AMFS_L3_HIT``

       Number of AMFS hits in Device Cache
     - ``DeviceCacheProfile``
   * - ``AMFS_L3_WRITE``

       Number of AMFS writes to Device Cache
     - ``DeviceCacheProfile``
   * - ``COLOR_L3_ACCESS``

       Number of Device Cache requests resulting from color local cache misses
     - ``DeviceCacheProfile``
   * - ``COLOR_L3_HIT``

       Number of Device Cache requests resulting from color local cache miss which hits in Device Cache
     - ``DeviceCacheProfile``
   * - ``ICACHE_L3_HIT``

       Number of XVE instruction cache requests that hit the Device Cache
     - ``DeviceCacheProfile``
   * - ``ICACHE_L3_READ``

       Number of Device Cache read requests coming from XVE Instruction Cache
     - ``DeviceCacheProfile``
   * - ``LOAD_STORE_CACHE_L3_HIT``

       Number of Load Store Cache requests to Device Cache that are hit
     - ``DeviceCacheProfile``
   * - ``LOAD_STORE_CACHE_L3_READ``

       Number of cacheline read requests from the Load Store Cache to Device Cache
     - ``DeviceCacheProfile``
   * - ``LOAD_STORE_CACHE_L3_WRITE``

       Number of cacheline write requests from the Load Store Cache to Device Cache
     - ``DeviceCacheProfile``
   * - ``SAMPLER_L3_HIT``

       Number of Device Cache hit requests resulting from sampler local cache miss which hits in Device Cache
     - ``DeviceCacheProfile``
   * - ``SAMPLER_L3_READ``

       Number of Device Cache read requests resulting from sampler local cache miss
     - ``DeviceCacheProfile``
   * - ``Z_L3_ACCESS``

       Number of Device Cache requests resulting from Z local cache misses
     - ``DeviceCacheProfile``
   * - ``Z_L3_HIT``

       Number of Device Cache requests resulting from Z local cache miss which hits in Device Cache
     - ``DeviceCacheProfile``
   * - ``L3_BUSY``

       Percentage of time in which Device Cache request queue has one or more requests pending
     - ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_INPUT_AVAILABLE``

       Percentage of time in which Device Cache bank has input requests
     - ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_OUTPUT_READY``

       Percentage of time in which Device Cache bank has output ready
     - ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``L3_SUPERQ_FULL``

       Percentage of time in which all slots in Device Cache request queue are waiting for data return / response
     - ``DeviceCacheProfile``, ``MemoryProfile``, ``VectorEngineProfile``

GPU Memory
----------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``COMPRESSOR_INPUT``

       Number of 256B writes to compressible surfaces at the input of the compressor
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``COMPRESSOR_OUTPUT``

       Number of 256B writes to compressible surfaces at the output of the compressor
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``TLB_MISS``

       Number of misses from all TLBs and all engines
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``GPU_MEMORY_REQUEST_QUEUE_FULL``

       Percentage of time in which SQ is filled above a threshold (usually 48 entries)
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``GPU_MEMORY_BYTE_READ``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) read bytes
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``GPU_MEMORY_BYTE_WRITE``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) write bytes
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``GPU_MEMORY_BYTE_READ_RATE``

       Device local memory (HBM, GDDR, LPDDR, etc.) read bandwidth
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``GPU_MEMORY_BYTE_WRITE_RATE``

       Device local memory (HBM, GDDR, LPDDR, etc.) write bandwidth
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``, ``VectorEngineProfile``
   * - ``HOST_TO_GPUMEM_TRANSACTION_READ``

       Number of host 64B reads to GPU local (HBM) memory (downstream over an interface such as pcie, cxl or an integrated fabric as applicable to the respective product segment)
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``HOST_TO_GPUMEM_TRANSACTION_WRITE``

       Number of host 64B writes to GPU local (HBM) memory (downstream over an interface such as pcie, cxl or an integrated fabric as applicable to the respective product segment)
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``SYSMEM_TRANSACTION_READ``

       Number of system memory 64B reads (upstream over an interface such as pcie, cxl or an integrated fabric as applicable to the respective product segment)
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``SYSMEM_TRANSACTION_WRITE``

       Number of system memory 64B writes (upstream over an interface such as pcie, cxl or an integrated fabric as applicable to the respective product segment)
     - ``RenderBasic``, ``ComputeBasic``, ``MemoryProfile``
   * - ``GPU_MEMORY_L3_READ``

       Number of GTI memory reads from Device Cache caused by Device Cache misses
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``
   * - ``GPU_MEMORY_L3_WRITE``

       Number of GTI memory writes from Device Cache caused by Device Cache invalidations
     - ``ComputeBasic``, ``DeviceCacheProfile``, ``MemoryProfile``
   * - ``GPU_MEMORY_ACTIVE``

       Percentage of time in which device local memory (HBM, GDDR, LPDDR, etc.) is active
     - ``DepthProfile``
   * - ``GPU_MEMORY_READ``

       Number of GTI memory reads
     - ``DeviceCacheProfile``
   * - ``GPU_MEMORY_WRITE``

       Number of GTI memory writes
     - ``DeviceCacheProfile``
   * - ``GPU_MEMORY_32B_TRANSACTION_READ``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) non coherent reads (32B)
     - ``MemoryProfile``
   * - ``GPU_MEMORY_32B_TRANSACTION_WRITE``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) non coherent writes (32B)
     - ``MemoryProfile``
   * - ``GPU_MEMORY_64B_TRANSACTION_READ``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) non coherent reads (64B)
     - ``MemoryProfile``
   * - ``GPU_MEMORY_64B_TRANSACTION_WRITE``

       Number of device local memory (HBM, GDDR, LPDDR, etc.) non coherent writes (64B)
     - ``MemoryProfile``

Command Streamer and Copy Engine
--------------------------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``GPU_BUSY``

       Percentage of time in which GPU is not idle including all GPU engines
     - ``RenderBasic``, ``ComputeBasic``, ``DepthProfile``, ``DeviceCacheProfile``, ``MemoryProfile``, ``RenderPipeProfile``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``
   * - ``COMMAND_PARSER_COMPUTE_ENGINE_BUSY``

       Percentage of time in which there is a context loaded and active on the compute queue
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``COMMAND_PARSER_RENDER_ENGINE_BUSY``

       Percentage of time in which there is a context loaded and active on the 3D queue
     - ``RenderBasic``, ``ComputeBasic``, ``VectorEngineProfile``
   * - ``COMMAND_PARSER_COPY_ENGINE_BUSY``

       Percentage of time in which there is a context loaded and active on the copy queue
     - ``RenderBasic``, ``ComputeBasic``
   * - ``COMMAND_PARSER_COMPUTE_ENGINE_DISPATCH_KERNEL_COUNT``

       Number of compute walker commands parsed on the compute engine
     - ``ComputeBasic``, ``VectorEngineProfile``
   * - ``COMMAND_PARSER_FLUSH_COUNT``

       Number of Stalling flushes at the top of the pipe
     - ``ComputeBasic``
   * - ``COMMAND_PARSER_RENDER_ENGINE_DISPATCH_KERNEL_COUNT``

       Number of compute walker commands parsed on the 3D engine
     - ``ComputeBasic``, ``VectorEngineProfile``
   * - ``COPY_ENGINE_READ_REQUEST``

       Number of read requests sent by the Copy Engine
     - ``MemoryProfile``
   * - ``COPY_ENGINE_REQUEST_STALL``

       Percentage of time in which copy engine is stalled on the memory path due to stall on the read or write path when there are commands pending within copy engine
     - ``MemoryProfile``
   * - ``COPY_ENGINE_WRITE_REQUEST``

       Number of write requests sent by the Copy Engine
     - ``MemoryProfile``
   * - ``COMMAND_PARSER_RENDER_ENGINE_DRAW_COUNT``

       Number of DRAW and MESH commands parsed on the 3D engine
     - ``RenderPipeProfile``

Graphics Pipeline
-----------------

These metrics count work of the 3D and ray tracing pipelines; compute kernels leave most of them at 0.

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - Metric
     - Groups
   * - ``IA_VERTEX``

       Number of vertices in a draw
     - ``RenderBasic``, ``ComputeBasic``, ``DepthProfile``, ``DeviceCacheProfile``, ``MemoryProfile``, ``RenderPipeProfile``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``
   * - ``CLIPPER_INPUT_VERTEX``

       Number of Clipper input vertices
     - ``RenderBasic``
   * - ``CLIPPER_PRIMITIVE_CULL``

       Number of Clipper early cull primitives
     - ``RenderBasic``
   * - ``CLIPPER_PRIMITIVE_FAR_NEAR_CLIP``

       Number of primitives clipped by Clipper due to near/far planes
     - ``RenderBasic``
   * - ``CLIPPER_PRIMITIVE_OUTPUT``

       Number of primitives going out of Clipper, must clip plus the trivial accept. Must clip primitives are counted one time only
     - ``RenderBasic``
   * - ``IA_PRIMITIVE``

       Number of primitives in a draw
     - ``RenderBasic``
   * - ``STREAMOUT_OUTPUT_VERTEX_COUNT``

       Number of vertices written by SOL
     - ``RenderBasic``
   * - ``STRIPSFAN_OBJECT_COUNT``

       Number of objects exiting Stripsfan stage
     - ``RenderBasic``
   * - ``STRIPSFAN_OBJECT_CULL``

       Number of simple and complex culled objects in Stripsfan stage
     - ``RenderBasic``
   * - ``STREAMOUT_PRIMITIVE_WRITE_COUNT``

       Number of objects data written to memory, read from pipe 0 only
     - ``RenderBasic``
   * - ``CLIPPER_INPUT_AVAILABLE``

       Percentage of time in which Clipper has input available (from Vertex Shader or SOL)
     - ``RenderPipeProfile``
   * - ``CLIPPER_OUTPUT_READY``

       Percentage of time in which Clipper output ready
     - ``RenderPipeProfile``
   * - ``CLIPPER_TRANSACTION_OUTPUT``

       Number of elements pushed by Clipper into Stripsfan stage
     - ``RenderPipeProfile``
   * - ``STREAMOUT_PRIMITIVE_STORAGE_NEED``

       Number of objects data that needed beyond the current allocated space, read from pipe 0 only
     - ``RenderPipeProfile``
   * - ``STRIPSFAN_OUTPUT_READY``

       Percentage of time in which in which geometry pipeline output is ready
     - ``RenderPipeProfile``
   * - ``VERTEX_FETCH_INPUT_AVAILABLE``

       Percentage of time in which Vertex Fetch input is available
     - ``RenderPipeProfile``
   * - ``VERTEX_FETCH_OUTPUT_READY``

       Percentage of time in which Vertex Fetch output is ready
     - ``RenderPipeProfile``
   * - ``VS_OUTPUT_READY``

       Percentage of time in which Vertex Shader output is ready
     - ``RenderPipeProfile``
   * - ``RASTERIZER_SAMPLE_OUTPUT``

       Number of lit samples emitted by Rasterizer
     - ``RenderBasic``, ``ComputeBasic``, ``DepthProfile``, ``DeviceCacheProfile``, ``MemoryProfile``, ``RenderPipeProfile``, ``RTProfile``, ``VectorEngineProfile``, ``VectorEngineStalls``
   * - ``PIXEL_2X2_LIT_POST_RASTERIZER_EARLY_DEPTH``

       Number of promoted 2x2 that are lit from Rasterizer
     - ``DepthProfile``
   * - ``PIXEL_2X2_LIT_POST_RASTERIZER_LATE_DEPTH``

       Number of non-promoted 2x2 that are lit from Rasterizer
     - ``DepthProfile``
   * - ``RASTERIZER_INPUT_AVAILABLE``

       Percentage of time in which Rasterizer input is available
     - ``RenderPipeProfile``
   * - ``RASTERIZER_OUTPUT_READY``

       Percentage of time in which where rasterizer output is ready
     - ``RenderPipeProfile``
   * - ``RASTERIZER_TRANSACTION_OUTPUT``

       Number of transactions pushed from Rasterizer to the Z pipe
     - ``RenderPipeProfile``
   * - ``EARLY_DEPTH_STENCIL_TEST_FAIL_NP``

       Number of non-promoted 2x2 that failed Depth / Stencil before the Pixel Shader that were previously ambiguous at HiZ
     - ``DepthProfile``
   * - ``EARLY_DEPTH_STENCIL_TEST_FAIL_P``

       Number of promoted 2x2 that failed Depth / Stencil that were previously ambiguous at HiZ
     - ``DepthProfile``
   * - ``HIZ_DEPTH_TEST_AMBIG_NP``

       Number of non-promoted 2x2 that are ambiguous by hierarchical depth test
     - ``DepthProfile``
   * - ``HIZ_DEPTH_TEST_AMBIG_P``

       Number of promoted 2x2 that are ambiguous by hierarchical depth test
     - ``DepthProfile``
   * - ``HIZ_DEPTH_TEST_FAIL_NP``

       Number of non-promoted 2x2 that failed the hierarchical depth test
     - ``DepthProfile``
   * - ``HIZ_DEPTH_TEST_FAIL_P``

       Number of promoted 2x2 that failed the hierarchical depth test
     - ``DepthProfile``
   * - ``HIZ_DEPTH_TEST_PASS_P``

       Number of promoted 2x2 passed by hierarchical depth test
     - ``DepthProfile``
   * - ``HIZ_SUBSPAN_LATENCY_FIFOFULL``

       Percentage of time in which for which HiZ latency hiding structure full
     - ``DepthProfile``
   * - ``IZ_OUTPUT_READY``

       Percentage of time in which IZ has requests to different clients
     - ``DepthProfile``
   * - ``IZ_SUBSPAN_LATENCY_FIFOFULL``

       Percentage of time in which for which IZ latency hiding structure full
     - ``DepthProfile``
   * - ``POSTPS_DEPTH_STENCIL_TEST_FAIL``

       Number of 2x2 that were lit from Rasterizer but failed the depth or stencil test post shader
     - ``DepthProfile``
   * - ``PIXEL_BLEND``

       Number of blended pixels written to all render targets
     - ``RenderBasic``
   * - ``PIXEL_WRITE``

       Number of pixels written to all render targets
     - ``RenderBasic``
   * - ``POSTPS_ALPHA_TEST_FAIL``

       Number of 2x2 that were lit from Rasterizer but failed the Alpha Test or Alpha-to-Coverage
     - ``RenderBasic``
   * - ``SHADER_PIX_KILL``

       Number of pixels that were lit from Rasterizer but killed by the Pixel Shader or OOB Check
     - ``RenderBasic``
   * - ``RENDER_CACHE_ALLOC``

       Number of Render Cache allocations
     - ``DepthProfile``
   * - ``RENDER_CACHE_HIT``

       Number of Render Cache hits
     - ``DepthProfile``
   * - ``RENDER_CACHE_READ``

       Number of Render Cache reads
     - ``DepthProfile``
   * - ``RENDER_CACHE_WRITE``

       Number of Render Cache writes
     - ``DepthProfile``
   * - ``COLOR_PIPE_CACHE_LATENCY1_STALL``

       Percentage of time in which Color Pipeline stalled due to MultiSampling Cache latency hiding structure full
     - ``RenderPipeProfile``
   * - ``COLOR_PIPE_CACHE_LATENCY2_STALL``

       Percentage of time in which Color Pipeline stalled due to Render Cache latency hiding structure full
     - ``RenderPipeProfile``
   * - ``PIXEL_POST_PROCESS_INPUT_AVAILABLE``

       Percentage of time in which Color Pipeline input ready
     - ``RenderPipeProfile``
   * - ``PIXEL_POST_PROCESS_OUTPUT_READY``

       Percentage of time in which Color Pipeline pixel output ready
     - ``RenderPipeProfile``
   * - ``PS_OUTPUT_AVAILABLE``

       Percentage of time in which Pixel Shader data is available
     - ``RenderPipeProfile``
   * - ``RENDER_CACHE_INPUT_AVAILABLE``

       Percentage of time in which Render Cache input available
     - ``RenderPipeProfile``
   * - ``RENDER_CACHE_OUTPUT_READY``

       Percentage of time in which Render Cache output ready
     - ``RenderPipeProfile``
   * - ``SAMPLER_2X2_READ``

       Number of 2x2 texel block requested from Sampler
     - ``RenderBasic``
   * - ``SAMPLER_TEXTURE_CACHE_ACCESS``

       Number of Sampler L1 requests
     - ``RenderBasic``
   * - ``SAMPLER_TEXTURE_CACHE_MISS``

       Number of Sampler L1 misses
     - ``RenderBasic``
   * - ``SAMPLER_ACTIVE``

       Percentage of time in which Sampler pipeline active
     - ``RenderPipeProfile``
   * - ``SAMPLER_INPUT_AVAILABLE``

       Percentage of time in which Sampler input is available
     - ``RenderPipeProfile``
   * - ``SAMPLER_MEMORY_LATENCY_STALL``

       Percentage of time in which Sampler stalled due to latency hiding structure full
     - ``RenderPipeProfile``
   * - ``SAMPLER_OUTPUT_READY``

       Percentage of time in which Sampler output is ready
     - ``RenderPipeProfile``
   * - ``RT_BVH_CACHE_MISS``

       Number of BVH cache misses
     - ``RenderBasic``, ``RTProfile``
   * - ``RT_INPUT_MESSAGE_RAY_COUNT``

       Number of valid SIMD lanes in the TraceRay message
     - ``RenderBasic``, ``RTProfile``
   * - ``RT_ANY_HIT_THREAD_RAY_DISPATCH``

       Number of any hit threads dispatched
     - ``RTProfile``
   * - ``RT_CALLABLE_THREAD_RAY_DISPATCH``

       Number of BTD shader spawn requests dispatched
     - ``RTProfile``
   * - ``RT_CLOSEST_HIT_THREAD_RAY_DISPATCH``

       Number of closest hit threads dispatched
     - ``RTProfile``
   * - ``RT_INPUT_MESSAGE_SPAWN_COUNT``

       Number of valid SIMD lanes in the Spawn message
     - ``RTProfile``
   * - ``RT_INSTANCE_NODE_RAY_COUNT``

       Number of instance BVH nodes processed by the traversal function
     - ``RTProfile``
   * - ``RT_INTERNAL_NODE_RAY_COUNT``

       Number of internal BVH nodes processed by the traversal function
     - ``RTProfile``
   * - ``RT_INTERSECTION_THREAD_RAY_DISPATCH``

       Number of intersection threads dispatched
     - ``RTProfile``
   * - ``RT_MESSAGE_STALL``

       Percentage of time in which Ray Tracing message input is stalled while accepting input
     - ``RTProfile``
   * - ``RT_MISS_THREAD_RAY_DISPATCH``

       Number of miss threads dispatched
     - ``RTProfile``
   * - ``RT_PROCEDURAL_NODE_RAY_COUNT``

       Number of procedural BVH nodes processed by the traversal function
     - ``RTProfile``
   * - ``RT_QUAD_LEAF_RAY_COUNT``

       Number of triangle BVH nodes processed by the traversal function
     - ``RTProfile``
   * - ``RT_QUAD_TEST_RAY_COUNT``

       Number of nodes processed that use ray-quad intersection pipeline
     - ``RTProfile``
   * - ``RT_REQUEST_COLLISION``

       Percentage of time in which Ray Tracing Leaf has two Traversal inputs valid
     - ``RTProfile``
   * - ``RT_TRANSFORM_RAY_COUNT``

       Number of only HW transform accesses
     - ``RTProfile``
   * - ``RT_TRAVERSAL_INPUT_RAY_COUNT``

       Number of Ray Tracing Traversal input rays
     - ``RTProfile``
   * - ``RT_TRAVERSAL_OUTPUT_RAY_COUNT``

       Number of Ray Tracing Traversal output rays
     - ``RTProfile``
   * - ``RT_TRAVERSAL_STALL``

       Percentage of time in which Ray Tracing Frontend is stalled by Traversal
     - ``RTProfile``
   * - ``RT_TRAVERSAL_STEP_RAY_COUNT``

       Number of BVH nodes processed
     - ``RTProfile``
   * - ``AMFS_STALL_ALL_INPUT``

       Percentage of time in which AMFS stalls at both of the color pipe inputs
     - ``RenderPipeProfile``
   * - ``AMFS_STALL_ANY_INPUT``

       Percentage of time in which AMFS stalls at any of the color pipe inputs
     - ``RenderPipeProfile``
   * - ``AMFS_4X4_SHADING_REQUEST``

       Number of Shading Request (evaluate) messages processed by AMFS
     - ``RenderPipeProfile``
   * - ``AMFS_CACHE_HIT``

       Number of hits in the AMFS cache
     - ``RenderPipeProfile``
   * - ``AMFS_CACHE_MISS``

       Number of cache misses in AMFS
     - ``RenderPipeProfile``
   * - ``URB_READ``

       Number of URB reads
     - ``RenderBasic``
   * - ``URB_WRITE``

       Number of URB writes
     - ``RenderBasic``
   * - ``URB_CROSS_SLICE_READ``

       Number of URB reads occurred from cross slices
     - ``DepthProfile``

.. END GENERATED TABLES

Other GPUs
==========

* Other GPUs have other metrics and groups. For example, Intel(R) Arc(TM) A-Series Graphics uses ``XveActive`` and ``XveStall`` instead of ``XVE_ACTIVE`` and ``XVE_STALL``.
* The lists for every GPU are in the `Intel(R) Metrics Discovery documentation <https://github.com/intel/metrics-discovery/tree/master/docs>`_, one ``metric_info`` file per GPU. Not every group in these files can be used for per-kernel collection.
* In a C or C++ program, ``ptiMetricsGetMetricGroups`` and ``ptiMetricsGetMetricsProperties`` list the groups and metrics of your GPU, with each group's type (see :doc:`metrics`).

If a Name Is Rejected
=====================

* ``PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND``: a name is misspelled or is not available for per-kernel collection on this GPU.
* ``PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP``: each name exists, but no single group has them all. Check the **Groups** column.
