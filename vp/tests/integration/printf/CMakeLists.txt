add_executable(printf main.c entry.S)
target_link_libraries(printf PRIVATE "-Wl,-e_reset_vector")
