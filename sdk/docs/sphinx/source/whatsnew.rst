==============
What's New
==============

Version 0.9.0
---------------

* Added function call(s) providing the timestamp and allowing the user to provide
  their own timestamp via a callback.
* Windows 11 support added.
* Various bug fixes and improvements.

Version 0.8.0 
---------------

* Added the ability to link against older, unsupported L0 loader and gracefully report unsupported.
* Various bug fixes and improvements.

Version 0.7.0
---------------

Implements the new functionality of Local collection. It enables starting and stopping collection anytime-anywhere in an application when run on the system with installed Level-Zero runtime supporting `1.9.0 specification <https://spec.oneapi.io/releases/index.html#level-zero-v1-9-0>`_ and higher.

Local collection functionality is transparent and controlled via ``ptiViewEnable`` and ``ptiViewDisable`` calls, where the first ``ptiViewEnable`` (or several of them) called at any place start the Local collection and the last ``ptiViewDisable`` (or several of them, paired with preceding ``ptiViewEnable`` calls) stop the Local collection.
Outside of Local collection regions of interest, PTI SDK maintains zero overhead by not issuing any calls or collecting any data.

On systems with Level-Zero version lower than 1.9.0 **PTI SDK** still operates as before its version 0.7.0: tracing runtime calls and causing the overhead outside of ``ptiViewEnable`` - ``ptiViewDisable`` regions, but reporting data only for ``ptiViewEnable`` - ``ptiViewDisable`` regions.
