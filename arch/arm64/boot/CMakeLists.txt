file(GLOB BOOT_SOURCES "*.S")
file(GLOB ARCH_SOURCES "../*.S")
target_sources(arcline.elf PRIVATE ${BOOT_SOURCES} ${ARCH_SOURCES})