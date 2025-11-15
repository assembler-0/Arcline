file(GLOB LIB_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${LIB_SOURCES})
