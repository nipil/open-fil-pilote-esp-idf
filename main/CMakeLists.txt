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
        "api_accounts.c"
        "api_zones.c"
        "api_mgmt.c"
        "api_plannings.c"
        "storage.c"
        "str.c"
        "console.c"
        "s2p_595.c"
        "fwupd.c"
    INCLUDE_DIRS
        "."
    EMBED_FILES
        "../ota.bin"
)

#[[This define speeds up iterations

set_source_files_properties(
    main.c
    PROPERTIES COMPILE_FLAGS
    -DOFP_DISABLE_NETWORKING
)

]]