=======
Build
=======

Build the pti library, tests, and samples: 

.. code-block:: bash

   source <path_to_oneapi>/setvars.sh
   cd sdk
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/icpx_toolchain.cmake ..
   make -j

--------------
Installation 
--------------

Install manually-built library:

.. code-block:: bash

   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/icpx_toolchain.cmake -DBUILD_TESTING=OFF ..
   make -j
   cmake --install . --config Release --prefix "../out"
