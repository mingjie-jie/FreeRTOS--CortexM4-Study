cmake_minimum_required(VERSION 3.12)
project(FreeRTOS_CortexM4_Study_Examples C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TEACHING_EXAMPLES
    pure_c_scheduler
    context_simulation
    register_stack_walkthrough
    delay_state_machine
)
foreach(example IN LISTS TEACHING_EXAMPLES)
    add_executable(${example} examples/${example}/main.c)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${example} PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
endforeach()
enable_testing()
foreach(example IN LISTS TEACHING_EXAMPLES)
    add_test(NAME ${example}_test COMMAND ${example})
endforeach()
