cmake_minimum_required(VERSION 3.20)
project(Arcline
    LANGUAGES C CXX ASM
    VERSION 0.0.1
    DESCRIPTION "Arcline - A POSIX-compliant operating system for arm64 architecture"
    HOMEPAGE_URL "https://github.com/assembler-0/Arcline"
)

# --- Toolchain and Target ---
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR "llvm-ar")
set(CMAKE_RANLIB "llvm-ranlib")
set(CMAKE_OBJCOPY "llvm-objcopy")
set(CMAKE_OBJDUMP "llvm-objdump")
set(CMAKE_LINKER clang)

# --- Compiler and Linker Flags ---
set(TARGET_ARCH "aarch64-elf")

set(CMAKE_C_FLAGS "-target ${TARGET_ARCH} -ffreestanding -nostdlib -Wall -Wextra -g -O0 -mcpu=cortex-a57 -mno-outline-atomics -fno-stack-protector -mgeneral-regs-only")
set(CMAKE_ASM_FLAGS "-target ${TARGET_ARCH} -mcpu=cortex-a57")

# Linker script and configurable kernel load address
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/arch/arm64/boot/linker.ld)

# Allow overriding the kernel load address without editing the linker script.
# Default matches QEMU aarch64 virt -kernel load addr (0x40080000).
set(KERNEL_LOAD_ADDR "0x40080000" CACHE STRING "Kernel load/entry address (e.g., 0x40080000 for QEMU virt)")

set(CMAKE_EXE_LINKER_FLAGS "-target ${TARGET_ARCH} -nostdlib -T ${LINKER_SCRIPT} -fuse-ld=lld -Wl,--no-undefined -Wl,-defsym=KERNEL_LOAD_ADDR=${KERNEL_LOAD_ADDR}")

# --- VMM higher-half kernel base (virtual address) ---
set(KERNEL_VIRT_BASE "0xFFFFFF8000000000" CACHE STRING "Kernel higher-half virtual base (e.g., 0xFFFFFF8000000000 for 48-bit VA)")

# --- Executable ---
add_executable(arcline.elf "") # No sources here, they are added by subdirectories

target_include_directories(arcline.elf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/arch/arm64/include
)

# Expose the configured higher-half base to C code as VMM_KERNEL_VIRT_BASE
target_compile_definitions(arcline.elf PUBLIC VMM_KERNEL_VIRT_BASE=${KERNEL_VIRT_BASE})

# --- Subdirectories ---
add_subdirectory(arch)
add_subdirectory(init)
add_subdirectory(kernel)
add_subdirectory(mm)
add_subdirectory(fs)
add_subdirectory(drivers)
add_subdirectory(lib)

# --- CMake Modules ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(ccache)

# --- Post-build command ---
add_custom_command(
    TARGET arcline.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary arcline.elf arcline.bin
)

# --- Custom run target ---
add_custom_target(run
    COMMAND qemu-system-aarch64
            -M virt
            -cpu cortex-a57
            -serial stdio
            -kernel ${CMAKE_BINARY_DIR}/arcline.bin
            -m 1024
    DEPENDS arcline.elf
    COMMENT "Running Arcline in QEMU"
)
