// esp_log.h

#pragma once


#define ESP_LOGI(tag, fmt, ...)	\
	printf(tag); \
	printf(fmt, __VA_ARGS__); \
	printf("\n"); \

