file(GLOB KERNEL_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${KERNEL_SOURCES})
