/**
 * @file trd_test.hpp
 * @author qingyu
 * @brief Kalman filter test thread
 * @version 0.1
 * @date 2026-06-28
 */

#pragma once

#include <stdint.h>

namespace thread::test {
    void thread_init();
    void thread_start(uint8_t prio = 10);
};
