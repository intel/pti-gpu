#####################
Introduction
#####################

This is the PTI SDK library.

PTI SDK will be a library for developing profiling tools for applications built on top of oneAPI and running on Intel GPUs.

Today's SDK provides ``pti_view`` library with an API to trace various tasks of application running on Intel GPU. While the library implementation uses low-level tracing APIs of SYCL run-time and Level-Zero, its own API is high-level.

PTI library is being built on the experience of PTI-GPU tools and samples and reuses that code with some modification.

As for the project organization here - SDK folder is self-contained and independent from the rest of repository.

One of the objectives is to extend functionality of the PTI library and with time to transform today's PTI-GPU project to an SDK.

This project is in active development. We decided to open it at this early stage to benefit from feedback and criticism of interested parties and early adopters.

