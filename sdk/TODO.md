# TODO list for Profiling Tools Interfaces SDK

1. Make a documentation
2. Clean and optimize View records
    - Compact them, e.g., remove redundant fields, compact other fields.
    - If decided to keep - make records fields representing context, device etc. - back-end agnostic
    - Remove (?) `_process_id` field
3. Implement Start/Stop functionality, so that collection could be triggered at arbitrary place of
a process
4. Clarify and properly define `pti_view_memcpy_type` and `pti_view_memory_type`

