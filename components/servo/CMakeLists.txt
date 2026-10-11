idf_component_register(SRCS "servo.cpp" "pca9685.cpp" INCLUDE_DIRS "include" "." REQUIRES core esp_driver_ledc esp_driver_i2c)
