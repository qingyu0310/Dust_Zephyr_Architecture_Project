# SPDX-License-Identifier: Apache-2.0

# 板卡专属全局 include（根 CMakeLists 的 foreach 统一 zephyr_include_directories）
set(BOARD_GLOBAL_INCLUDES
  "${ZEPHYR_USER_DIR}/platform/cmsis"
  "${ZEPHYR_USER_DIR}/../modules/hal/cmsis/CMSIS/Core/Include"
)

set(BOARD_FLASH_RUNNER openocd)
set(OPENOCD "C:/Program Files/Path/openocd/bin/openocd.exe" CACHE FILEPATH "")

set(BOARD_CFG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROJ_DIR}/boards")

macro(app_set_runner_args)
  get_property(_ocd_done GLOBAL PROPERTY _OCD_STM32_DONE)
  if(NOT _ocd_done)
    set_property(GLOBAL PROPERTY _OCD_STM32_DONE TRUE)
    board_runner_args(openocd "--openocd-search=C:/Program Files/Path/openocd/share/openocd/scripts")
    file(GLOB ocd_cfg "${BOARD_CFG_DIR}/*/${BOARD_CFG}/openocd.cfg")
    if(ocd_cfg)
      board_runner_args(openocd "--config=${ocd_cfg}")
    endif()
  endif()
endmacro()
