PTI Documentation
==================

.. warning::
   **DRAFT DOCUMENTATION** - This documentation is currently in draft status and subject to change.

**Source Repository:** https://github.com/intel/pti-gpu/tree/master/sdk

Overview
--------

Intel Profiling Tools Interfaces for GPU SDK (hereafter PTI SDK) enables development of profiling tools for compute applications or frameworks using Intel oneAPI and running on Intel GPUs.
PTI SDK consists of a C API library and the supporting modules it relies on.

PTI SDK abstracts details of the runtime and driver stack and makes it easy to pick the desired level of profiling: for example, trace GPU device kernel execution, memory transfers,
SYCL Runtime calls, Level-Zero calls on the host, and/or collect GPU hardware metrics.

The API is the same for Linux and Windows operating systems and supports a range of client and data center Intel GPUs (see :doc:`Supported Hardware <systemreqs>`).

.. include:: toctree.rst

----

Notices and Disclaimers
========================

© Intel Corporation. Intel, the Intel logo and other Intel marks are trademarks of Intel Corporation or its subsidiaries. Other names and brands may be claimed as the property of others.

No license (express or implied, by estoppel or otherwise) to any intellectual property rights is granted by this document, with the sole exception that code included in this document is licensed subject to the Zero-Clause BSD open-source license (0BSD), http://opensource.org/licenses/0BSD.
