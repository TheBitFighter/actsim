include_directories(${ACTHEADERS_LOCATION})
include_directories(./)
include_directories(../)
include_directories(../simlib)

file(
    GLOB proj_SRC
    "*.cc"
)

# Add the main executable
add_executable(
    ${PROJECT_NAME}
    ${proj_SRC}
)

# link libaries to executable
target_link_libraries(
    ${PROJECT_NAME}
    ACTPASSLIB
    ACTLIB
    ACTCOMMON
    ACTASIMLIB
    TRACELIB
    ACTSCMLIB
    ACTSCMCLILIB
    dl
    edit
    m
    z
)

# check if we need to link for XYCE
if(DEFINED LINK_XYCE)
    target_link_libraries(
        ${PROJECT_NAME}
        XYCE_LIB
    )
endif()
