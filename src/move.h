#pragma once
#include "main.h"

constexpr int king_c_flag_mask[n_sides]={
~(c_flag_wl|c_flag_wr),
~(c_flag_bl|c_flag_br),
};
