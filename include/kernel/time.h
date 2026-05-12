#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>

uint8_t read_cmos_register(int reg);
uint8_t rtc_get_minutes();
uint8_t rtc_get_hours();
uint8_t rtc_get_day_of_month();
uint8_t rtc_get_seconds();
uint8_t rtc_get_month();
int rtc_get_year();
uint8_t rtc_get_century();
uint64_t get_unix_time();
uint64_t to_unix_time(int hours, int minutes, int seconds, int day, int month, int year);

#endif