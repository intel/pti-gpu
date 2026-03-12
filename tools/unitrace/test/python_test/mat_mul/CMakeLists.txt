
add_test(NAME mat_mul COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../run_test.py -c ${CMAKE_SOURCE_DIR} -t mat_mul ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/mat_mul/matMul.py)
