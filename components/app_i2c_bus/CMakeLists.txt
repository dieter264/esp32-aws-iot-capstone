idf_component_register(SRCS "bus.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES app_cmn esp_driver_i2c
                       PRIV_REQUIRES app_config_store)
