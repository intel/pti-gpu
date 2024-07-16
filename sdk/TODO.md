# TODO list for Profiling Tools Interfaces SDK

1. Make a documentation
2. Clean and optimize View records
    - Compact them, e.g., remove redundant fields, compact other fields.
    - If decided to keep - make records fields representing context, device etc. - back-end agnostic
    - Remove (?) `_process_id` field
3. Clarify and properly define `pti_view_memcpy_type` and `pti_view_memory_type`
4. Change type of _pci_address in all records to numerical.
5. Structurize uuid -- is currently a uint8_t array of PTI_MAX_DEVICE_UUID_SIZE.

