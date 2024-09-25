==============
Installation
==============

.. code-block:: bash

   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/icpx_toolchain.cmake -DBUILD_TESTING=OFF ..
   make -j
   cmake --install . --config Release --prefix "../out"
