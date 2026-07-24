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