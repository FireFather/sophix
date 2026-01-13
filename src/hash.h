#pragma once
#include <array>
#include "main.h"
#include "search.h"

enum : u8{
  hash_bound_unused,hash_lower,hash_upper,hash_exact,
};

enum : u8{
  z_keys_ep_flag=empty+1,z_keys_max_index,
};

struct zobkeys_t{
  std::array<std::array<u64,z_keys_max_index>,sq_limit> positions;
  std::array<u64,c_flag_max> c_flags;
  u64 side_flag;
};

union hash_data_t{
  struct{
    move_t move;
    i16 static_eval;
    i8 depth;
    u8 bound:2, iter:6;
  };

  u64 raw;
};

union phash_data_t{
  struct{
    i16 score_mg;
    i16 score_eg;
    u64 pushed_passers;
  };

  u64 raw[2];
};

hash_data_t get_hash(const search_data_t*);
phash_data_t set_phash(const position_t*,u64,int,int);
int adjust_hash(int,int);
int get_phash(const position_t*,phash_data_t*);
u64 init_hash(int);
void init_phash(int);
void reset_hash(search_data_t*);
void set_hash(const search_data_t*,move_t,int,int,int,int,int);
void set_hash_iteration();
extern zobkeys_t zobkeys;
