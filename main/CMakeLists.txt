idf_component_register(
    SRCS 
        "main.c" 
        "utils.c"
        "sntp.c"
        "uptime.c"
        "m_dns.c"
        "webserver.c"
        "ofp.c"
        "hw_m1e1.c"
        "hw_esp32.c"
        "api_hw.c"
    INCLUDE_DIRS
        "."
)

#[[This define speeds up iterations

set_source_files_properties(
    main.c
    PROPERTIES COMPILE_FLAGS
    -DOFP_DISABLE_NETWORKING
)

]]