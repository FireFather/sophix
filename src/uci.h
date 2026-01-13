#pragma once
#include "search.h"

enum : uint8_t{
  max_reduce_time=200,// Was 1000
  reduce_time=25,// Was 150
  reduce_time_percent=2,// Was 5
  max_time_moves_to_go=3
};

enum{
  max_moves_to_go=20,buffer_line_size=256,read_buffer_size=65536
};

inline char initial_fen[]="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
void uci();
void print_search_info(const move_t*);
