cmake_minimum_required(VERSION 3.22)

#
# stm_log — STM32 HAL 专用分级日志组件
#
# 用法（集成到 STM32 工程根 CMakeLists.txt）：
#   add_subdirectory(stm_log)
#   target_link_libraries(${YOUR_TARGET} stm_log)
#
# 依赖（本库自动传递，调用方无需再 link stm32cubemx）：
#   - STM32 HAL：stm_log.h 用 STM_LOG_HAL_HEADER 决定（默认 stm32f4xx_hal.h）
#   - CubeMX 生成的 stm32cubemx INTERFACE target（含 HAL 头文件 + 编译符号）

project(stm_log C)

# 是否自动传递 stm32cubemx（默认 ON）。
# 非标准 CubeMX 工程、或自定义 HAL target 命名时可关：-DSTM_LOG_LINK_CUBEMX=OFF
option(STM_LOG_LINK_CUBEMX
       "Link stm32cubemx target automatically (default ON)"
       ON)

add_library(stm_log STATIC
    stm_log.c
)

target_include_directories(stm_log PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 自动传递 HAL 依赖：调用方 link stm_log 时无需再显式 link stm32cubemx
if(STM_LOG_LINK_CUBEMX AND TARGET stm32cubemx)
    target_link_libraries(stm_log PUBLIC stm32cubemx)
endif()

option(STM_LOG_WITH_RTT "Provide SEGGER RTT as a transitive logging dependency" OFF)
if(STM_LOG_WITH_RTT)
    if(NOT TARGET segger_rtt)
        set(STM_LOG_RTT_SOURCE_DIR "" CACHE PATH "Local RTT repository root")
        option(STM_LOG_RTT_FETCH "Download RTT if local sources are unavailable" ON)
        set(STM_LOG_RTT_GIT_REPOSITORY "https://github.com/NingZiXi/RTT.git"
            CACHE STRING "SEGGER RTT Git repository (GitHub or Gitee mirror)")
        set(_stm_log_rtt_source "${STM_LOG_RTT_SOURCE_DIR}")
        if(NOT _stm_log_rtt_source)
            foreach(_candidate segger_rtt RTT)
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../${_candidate}/RTT/SEGGER_RTT.c")
                    get_filename_component(_stm_log_rtt_source
                        "${CMAKE_CURRENT_SOURCE_DIR}/../${_candidate}" ABSOLUTE)
                    break()
                endif()
            endforeach()
        endif()
        if(NOT _stm_log_rtt_source)
            if(NOT STM_LOG_RTT_FETCH)
                message(FATAL_ERROR "RTT is missing. Provide segger_rtt target, STM_LOG_RTT_SOURCE_DIR, sibling RTT/segger_rtt, or enable STM_LOG_RTT_FETCH.")
            endif()
            include(FetchContent)
            FetchContent_Declare(segger_rtt
                GIT_REPOSITORY "${STM_LOG_RTT_GIT_REPOSITORY}"
                GIT_TAG d6075232fb5a7127c36548f19eb8cd5befee69e7
            )
            FetchContent_MakeAvailable(segger_rtt)
            set(_stm_log_rtt_source "${segger_rtt_SOURCE_DIR}")
        endif()
        # 当前 RTT 仓库没有 CMake 入口，仅编译日志输出所需的核心实现。
        if(NOT TARGET segger_rtt)
            if(NOT EXISTS "${_stm_log_rtt_source}/RTT/SEGGER_RTT.c"
               OR NOT EXISTS "${_stm_log_rtt_source}/Config/SEGGER_RTT_Conf.h")
                message(FATAL_ERROR "Invalid RTT source directory: ${_stm_log_rtt_source}")
            endif()
            add_library(segger_rtt STATIC "${_stm_log_rtt_source}/RTT/SEGGER_RTT.c")
            target_include_directories(segger_rtt PUBLIC "${_stm_log_rtt_source}/RTT")
            set(STM_LOG_RTT_CONFIG_DIR "" CACHE PATH "Directory containing custom SEGGER_RTT_Conf.h")
            if(STM_LOG_RTT_CONFIG_DIR)
                if(NOT EXISTS "${STM_LOG_RTT_CONFIG_DIR}/SEGGER_RTT_Conf.h")
                    message(FATAL_ERROR "STM_LOG_RTT_CONFIG_DIR must contain SEGGER_RTT_Conf.h")
                endif()
                target_include_directories(segger_rtt PUBLIC "${STM_LOG_RTT_CONFIG_DIR}")
            else()
                target_include_directories(segger_rtt PUBLIC "${_stm_log_rtt_source}/Config")
            endif()
        endif()
        unset(_stm_log_rtt_source)
        unset(_candidate)
    endif()
    target_link_libraries(stm_log PUBLIC segger_rtt)
endif()
