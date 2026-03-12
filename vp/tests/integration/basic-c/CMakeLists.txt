add_executable(basic-c bootstrap.S sum.c main.c)
target_link_libraries(basic-c PRIVATE "-nostdlib")
