==============
Known Issues
==============

Significant overhead is currently expected for workloads that frequently submit work to the GPU.  Optimization efforts will continue both in the library as well as in the underlying layers of the software stack.  

For Local collection mode, Rolling drivers are currently required.  LTS drivers from 2024 and earlier do not support it, and instead require Level Zero tracing to be globally enabled.

Kernel name demangling is not supported on Windows.
