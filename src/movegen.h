#pragma once
#include "position.h"

int non_king_moves(const position_t*);
void check_evasion_moves(const position_t*,move_t*,int*);
void checks_and_material_moves(const position_t*,move_t*,int*);
void get_all_moves(const position_t*,move_t*,int*);
void king_moves(const position_t*,move_t*,int*);
void material_moves(const position_t*,move_t*,int*,int);
void quiet_moves(const position_t*,move_t*,int*);
