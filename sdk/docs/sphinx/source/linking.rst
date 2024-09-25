=========
Linking
=========

Use CMake ``find_package``

.. code-block:: cmake

   # set Pti_DIR if you install in a nonstandard location.
   set(Pti_DIR <path-to-pti>/lib/cmake/pti)
   find_package(Pti X.Y.Z)
   target_link_libraries(stuff PUBLIC Pti::pti_view)
