add_executable(mc-basic bootstrap.S main.c)
target_link_libraries(mc-basic PRIVATE "-nostdlib")
