file(GLOB SAN_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${SAN_SOURCES})
