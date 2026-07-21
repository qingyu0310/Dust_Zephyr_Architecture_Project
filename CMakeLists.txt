target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/Irq_handlers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/Init_entry.cpp
)
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)

zephyr_linker_sources(ROM_SECTIONS ../cmd/linker/tflm_init.ld)

add_subdirectory(thread)
