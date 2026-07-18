cmake_minimum_required(VERSION 3.22)

#
# stm_log — STM32 HAL 专用分级日志组件
#
# 用法（集成到 STM32 工程根 CMakeLists.txt）：
#   add_subdirectory(stm_log)
#   target_link_libraries(${YOUR_TARGET} PRIVATE stm_log)
#
# 依赖（由调用方 link，本库不强制）：
#   - STM32 HAL：stm32f4xx_hal.h / HAL_UART_Transmit / HAL_GetTick
#   - 一般 link stm32cubemx target 即可

project(stm_log C)

add_library(stm_log STATIC
    stm_log.c
)

target_include_directories(stm_log PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)