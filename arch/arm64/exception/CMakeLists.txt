file(GLOB EXCEPTION_SOURCES "*.S" "*.c")
target_sources(arcline.elf PRIVATE ${EXCEPTION_SOURCES})