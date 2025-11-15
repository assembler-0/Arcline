file(GLOB BOOT_SOURCES "*.S")
target_sources(arcline.elf PRIVATE ${BOOT_SOURCES})