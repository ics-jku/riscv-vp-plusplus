add_library(rvfi-dii
        rvfi_dii.cpp
		${HEADERS})

target_link_libraries(rvfi-dii rv64)
