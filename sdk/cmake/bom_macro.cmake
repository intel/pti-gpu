macro(bomline bomDir class dirPrefix tgtMatch)
  install(CODE "
    file(
      GLOB_RECURSE CMAKE_FILES_LIST
      ${tgtMatch}
    )
    message(STATUS \"BEFORE CMAKE\" ${class})
    foreach(FILE \${CMAKE_FILES_LIST})
        message(STATUS \"Installed file1: \${FILE}\")
	if (UNIX)
		execute_process(COMMAND cmake/bom_line.bash ${bomDir} ${class} ${dirPrefix} \${CMAKE_INSTALL_PREFIX} \${FILE})
	else()
		execute_process(COMMAND [[cmake\\bom_line_win.bat]] ${bomDir} ${class} ${dirPrefix} \${CMAKE_INSTALL_PREFIX} \${FILE})
	endif()
    endforeach()
    message(STATUS \"OUTSIDE CMAKE\" ${class})
    "
    COMPONENT Pti_Bom)
endmacro()


