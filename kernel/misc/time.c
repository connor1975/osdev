#include <stdint.h>
#include <kernel/common.h>

uint8_t read_cmos_register(int reg){
    outb(0x70,reg);
    return inb(0x71);
}

uint8_t rtc_get_seconds(){
    uint8_t seconds = read_cmos_register(0x00);
    seconds = ((seconds / 16) * 10) + (seconds & 0xf);
    return seconds;
}

uint8_t rtc_get_minutes(){
    uint8_t minutes = read_cmos_register(0x02);
    minutes = ((minutes / 16) * 10) + (minutes & 0xf);
    return minutes;
}

uint8_t rtc_get_hours(){
    uint8_t hours = read_cmos_register(0x04);
    hours = ((hours / 16) * 10) + (hours & 0xf);
    return hours;
}

uint8_t rtc_get_day_of_month(){
    uint8_t day = read_cmos_register(0x07);
    day = ((day / 16) * 10) + (day & 0xf);
    return day;
}

uint8_t rtc_get_month(){
    uint8_t month = read_cmos_register(0x08);
    month = ((month / 16) * 10) + (month & 0xf);
    return month;
}

uint8_t rtc_get_century(){
    int century = read_cmos_register(0x32);
    century = ((century / 16) * 10) + (century & 0xf);
    return century;
}

int rtc_get_year(){
    int year = read_cmos_register(0x09);
    year = ((year / 16) * 10) + (year & 0xf);
    year+=rtc_get_century() * 100;
    return year;
}

int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};

int is_leap_year(int year){
    if (year % 4 == 0) {
        if (year % 100 == 0) {
            return (year % 400 == 0);
        }
        return 1;
    }
    return 0;
}

int get_days_in_month(int month, int year){
    if(month == 1 && is_leap_year(year)) return 29;
    return days_in_month[month];
}

uint64_t to_unix_time(int hours, int minutes, int seconds, int day, int month, int year){
    uint64_t ret = 0;
    ret+= seconds;
    ret+=minutes * 60;
    ret+=(hours * 60) * 60;
    ret+=((day-1)*24) * 60 * 60;

    for(int i = 0; i < month-1; i++){
        int days = get_days_in_month(i,year);
        ret+=(days*24) * 60 * 60;
    }

    for(int i = 1970; i < year; i++){
        if(is_leap_year(i)) ret+=((366 * 24) * 60 * 60);
        else ret+=((365 * 24) * 60 * 60);
    }

    return ret;
}

uint64_t get_unix_time(){
    return to_unix_time(rtc_get_hours(),rtc_get_minutes(),rtc_get_seconds(),rtc_get_day_of_month(), rtc_get_month(),rtc_get_year());
}