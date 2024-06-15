include_directories(${ACTHEADERS_LOCATION})
include_directories(./)
include_directories(../)

file(
    GLOB simlib_SRC
    "*.cc"
)

# Add the main executable
add_library(
    ${SIMLIB_NAME}
    SHARED
    ${simlib_SRC}
)

# link libaries to executable
target_link_libraries(
    ${SIMLIB_NAME}
    ACTLIB
    ACTCOMMON
)
