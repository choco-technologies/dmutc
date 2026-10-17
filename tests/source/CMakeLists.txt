# test_dmutc_source: an external UTC source for test_dmutc - a library
# implementing the dmutc_read_source DIF with a fixed time. test_dmutc
# enables and disables it (a running application is no DIF implementation
# dmod looks at, so test_dmutc cannot be the source itself).
set(DMOD_MODULE_NAME        test_dmutc_source)
set(DMOD_MODULE_VERSION     ${PROJECT_VERSION})
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)
set(DMOD_MAL_IMPLS          )
set(DMOD_DIF_IMPLS          dmutc)

dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    source.c
)
target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_link_libraries(${DMOD_MODULE_NAME} dmutc_if)
