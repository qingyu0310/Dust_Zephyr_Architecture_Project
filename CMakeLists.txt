if(CONFIG_TRD_GPIO)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/gpio/trd_gpio.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/gpio
	)
endif()

if(CONFIG_TRD_CHASSIS)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/chassis/trd_chassis.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/chassis
	)
endif()

if(CONFIG_TRD_GIMBAL)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/gimbal/trd_gimbal.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/gimbal
	)
endif()

if(CONFIG_TRD_REMOTE)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/remote/trd_remote.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/remote
	)
endif()

if(CONFIG_TRD_IMU)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/imu/trd_imu.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/imu
	)
endif()

if(CONFIG_TRD_CAN_TX)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/can/trd_can_tx.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/can
	)
endif()

if(CONFIG_TRD_TFLM)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/tflm/trd_tflm.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/tflm
	)
endif()

if(CONFIG_TRD_PC)
	target_sources(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread/pc/trd_pc.cpp
	)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/pc
	)
endif()

if(CONFIG_TRD_TEST)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thread/test/trd_test.cpp)
	target_include_directories(app PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thread
		${CMAKE_CURRENT_SOURCE_DIR}/thread/test
	)
endif()
