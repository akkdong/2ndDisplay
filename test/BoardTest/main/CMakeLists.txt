idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp_driver_usb_serial_jtag esp_driver_i2c esp_wifi esp_netif esp_hosted)
