file(GLOB SCHED_SOURCES "*.c")
target_sources(arcline.elf PRIVATE ${SCHED_SOURCES})
