cmake_minimum_required(VERSION 4.1)
project(Arcline
        VERSION 0.0.1
        LANGUAGES CXX C ASM_NASM
        DESCRIPTION "Arcline - A POSIX-compliant operating system for arm64 architecture"
        HOMEPAGE_URL "https://github.com/assembler-0/Arcline"
)

set(CMAKE_C_STANDARD 23)

add_executable(Arcline init/main.c)
