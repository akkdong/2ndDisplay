// esp_log.h

#pragma once


#define ESP_LOGI(tag, fmt, ...)	\
	printf("[%s] ", tag); \
	printf(fmt, __VA_ARGS__); \
	printf("\n"); \


#define ESP_LOGE(tag, fmt, ...)	\
	printf("[%s] ", tag); \
	printf(fmt, __VA_ARGS__); \
	printf("\n"); \

