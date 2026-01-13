#pragma once
#include "position.h"

#ifdef _WIN64
#include <windows.h>
#else
#include <sys/time.h>
#endif

inline u64 time_in_ms(){
#ifdef _WIN64
  return GetTickCount64();
#else
  struct timeval timeValue;
  gettimeofday(&timeValue, NULL);
  return timeValue.tv_sec * 1000 + timeValue.tv_usec / 1000;
#endif
}

void sleep_ms(int);
char* to_str(move_t);
move_t str_to_m(const char*);
void print_board(const position_t* pos);
