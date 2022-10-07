idf_component_register(
    EMBED_TXTFILES
    "autosign.crt"
    "autosign.key"
)

add_custom_command(
    WORKING_DIRECTORY ${COMPONENT_DIR}
    OUTPUT ${COMPONENT_DIR}/autosign.crt ${COMPONENT_DIR}/autosign.key
    COMMAND python gen.py
    DEPENDS info.txt
)

add_custom_target(
    autosign
    DEPENDS autosign.crt autosign.key
)

add_dependencies(
    ${COMPONENT_TARGET} autosign
)

set_property(
    DIRECTORY ${COMPONENT_DIR}
    APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${COMPONENT_DIR}/autosign.crt ${COMPONENT_DIR}/autosign.key
)
