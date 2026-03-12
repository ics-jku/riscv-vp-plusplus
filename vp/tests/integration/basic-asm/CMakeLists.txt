add_executable(basic-asm sum.S)
target_link_libraries(basic-asm PRIVATE "-nostdlib")
