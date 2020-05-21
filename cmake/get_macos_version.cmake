function(get_macos_version)
    if (NOT APPLE)
        message(WARNING "The system is not macOS.")
        set(MACOS_VERSION 0 PARENT_SCOPE)
        return()
    endif ()

    execute_process(COMMAND sw_vers -productVersion
            RESULT_VARIABLE RET
            OUTPUT_VARIABLE STDOUT
            ERROR_VARIABLE STDERR)
    if (RET) # the command failed
        message(WARNING "Cannot get macOS version.")
        set(MACOS_VERSION 0 PARENT_SCOPE)
        return()
    endif ()

    set(MACOS_VERSION ${STDOUT} PARENT_SCOPE)
    message(VERBOSE "macOS version ${MACOS_VERSION}")

endfunction()