set(DMOD_MODULE_NAME        test_dmutc)
set(DMOD_MODULE_VERSION     ${PROJECT_VERSION})
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         2048)
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_dmutc.dmr)
set(DMOD_PRIORITY           0)

dmod_add_test(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmutc_test.c
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(${DMOD_MODULE_NAME} dmutc_if)

# Register the test module with ctest - this replays the same two steps CI
# runs (see .github/templates/ci.yml.template): install the test module's
# dependencies with dmf-get, then execute it through dmod_loader. The test's
# exit code equals its number of failed steps (see dmod_add_test).
find_program(DMF_GET_EXECUTABLE dmf-get HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)
find_program(DMOD_LOADER_EXECUTABLE dmod_loader HINTS ${DMOD_TOOLS_BIN_DIR} PATHS ENV PATH)

if(DMF_GET_EXECUTABLE AND DMOD_LOADER_EXECUTABLE)
    add_test(
        NAME ${DMOD_MODULE_NAME}
        COMMAND ${CMAKE_COMMAND} -E env DMOD_DMF_DIR=${DMOD_DMF_DIR}
            bash -c "${DMF_GET_EXECUTABLE} install -d ${DMOD_DMF_DIR}/${DMOD_MODULE_NAME}-local.dmd -y && ${DMOD_LOADER_EXECUTABLE} ${DMOD_DMF_DIR}/${DMOD_MODULE_NAME}.dmf"
    )
else()
    message(WARNING "dmf-get/dmod_loader not found - 'ctest' will not run ${DMOD_MODULE_NAME}. Build the tools first with -DDMOD_BUILD_TOOLS=ON")
endif()

if(DMOD_ARCH MATCHES "armv7")
    dmod_link_modules(${DMOD_MODULE_NAME} libsystemd dmosi)
    target_compile_definitions(${DMOD_MODULE_NAME} PRIVATE DMUTC_BOARD_TEST)
endif()

# Exercise the actual provider with an independent VFS/RTC model on the host.
if(DMOD_ARCH STREQUAL "x86_64" OR NOT DEFINED DMOD_ARCH)
    find_program(HOST_CC NAMES cc gcc REQUIRED)
    set(PROVIDER_TEST ${CMAKE_CURRENT_SOURCE_DIR}/rtc)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_rtc_provider
        COMMAND ${HOST_CC} -std=c11 -Wall -Wextra -Werror -pthread
            -I${PROVIDER_TEST}/stubs -I${CMAKE_CURRENT_SOURCE_DIR}/../include
            -I${CMAKE_CURRENT_SOURCE_DIR}/../src
            -I${DMOD_DMF_DIR}/inc/dmdrvi/include
            ${PROVIDER_TEST}/rtc_test.c ${CMAKE_CURRENT_SOURCE_DIR}/../src/dmutc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/rtc.c ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
            -o ${CMAKE_CURRENT_BINARY_DIR}/test_rtc_provider
        DEPENDS ${PROVIDER_TEST}/rtc_test.c ${PROVIDER_TEST}/stubs/dmod.h
            ${PROVIDER_TEST}/stubs/dmutc_defs.h ${PROVIDER_TEST}/stubs/dmosi.h
            ${PROVIDER_TEST}/stubs/libsystemd_service.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.h ${CMAKE_CURRENT_SOURCE_DIR}/../include/dmutc.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/dmutc.c ${CMAKE_CURRENT_SOURCE_DIR}/../src/rtc.c
            ${DMOD_DMF_DIR}/inc/dmdrvi/include/dmdrvi_ioctl.h)
    add_custom_target(rtc_provider_test_build ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_rtc_provider)
    add_test(NAME rtc_provider COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_rtc_provider)
endif()

add_subdirectory(source)
