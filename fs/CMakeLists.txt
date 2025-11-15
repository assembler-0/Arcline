file(GLOB FS_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${FS_SOURCES})
