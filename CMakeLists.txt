cmake_minimum_required(VERSION 3.20)
project(Arcline C ASM)

# --- Toolchain and Target ---
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR "llvm-ar")
set(CMAKE_RANLIB "llvm-ranlib")
set(CMAKE_OBJCOPY "llvm-objcopy")
set(CMAKE_OBJDUMP "llvm-objdump")
set(CMAKE_LINKER clang)

# --- Compiler and Linker Flags ---
set(TARGET_ARCH "aarch64-elf")

set(CMAKE_C_FLAGS "-target ${TARGET_ARCH} -ffreestanding -nostdlib -Wall -Wextra -g -O0 -mcpu=cortex-a57")
set(CMAKE_ASM_FLAGS "-target ${TARGET_ARCH} -mcpu=cortex-a57")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/arch/arm64/boot/linker.ld)
set(CMAKE_EXE_LINKER_FLAGS "-target ${TARGET_ARCH} -nostdlib -T ${LINKER_SCRIPT} -fuse-ld=lld")

# --- Executable ---
add_executable(arcline.elf "") # No sources here, they are added by subdirectories

target_include_directories(arcline.elf PUBLIC
    .
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/arch/arm64/include
)

# --- Subdirectories ---
add_subdirectory(arch)
add_subdirectory(init)
add_subdirectory(kernel)
add_subdirectory(mm)
add_subdirectory(fs)
add_subdirectory(drivers)


# --- Post-build command ---
add_custom_command(
    TARGET arcline.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary arcline.elf arcline.bin
    COMMENT "Creating kernel binary: arcline.bin"
)
