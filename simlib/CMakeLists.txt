add_subdirectory(src)

# replace file paths according to actual install paths in config
configure_file(actsim.conf actsim.conf @ONLY)