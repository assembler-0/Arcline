file(GLOB DRIVERS_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${DRIVERS_SOURCES})
