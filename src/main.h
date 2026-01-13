#pragma once
#include <algorithm>
#include <cstdint>
#include <iostream>

#ifdef _MSC_VER
#else
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

inline const std::string engine="Sophix";
inline const std::string version="1.0";
inline const std::string author="Milos Tatarevic";

#define SC static_cast
#define SO std::cout
#define SE std::endl

using i8=int8_t;
using i16=int16_t;
using i32=int32_t;
using i64=int64_t;

using u8=uint8_t;
using u16=uint16_t;
using u32=uint32_t;
using u64=uint64_t;

using move_t=u32;

enum :u8{
  white,black,n_sides
};

enum :u8{
  pawn,knight,bishop,rook,queen,king,n_pieces
};

enum :u8{
  a8,b8,c8,d8,e8,f8,g8,h8,
  a7,b7,c7,d7,e7,f7,g7,h7,
  a6,b6,c6,d6,e6,f6,g6,h6,
  a5,b5,c5,d5,e5,f5,g5,h5,
  a4,b4,c4,d4,e4,f4,g4,h4,
  a3,b3,c3,d3,e3,f3,g3,h3,
  a2,b2,c2,d2,e2,f2,g2,h2,
  a1,b1,c1,d1,e1,f1,g1,h1,
  n_sqs,sq_limit,
};

enum :u8{
  rank_8,rank_7,rank_6,rank_5,rank_4,rank_3,rank_2,rank_1,
  n_ranks
};

enum :u8{
  file_a,file_b,file_c,file_d,file_e,file_f,file_g,file_h,
  n_files
};

enum :u16{
  board_size=64,max_captures=32,max_killer_moves=2,max_moves=256,max_depth=100,ply_limit=128,empty=0xf
};

enum :u8{
  c_flag_wl=0x1,c_flag_wr=0x2,c_flag_bl=0x4,c_flag_br=0x8,c_flag_max=0x10
};

enum :u8{
  phase_mg=0,phase_eg=1,n_phases=2,phase_shift=7
};

enum{
  mate_score=30000,default_hash_size_in_mb=128,max_hash_size_in_mb=65536
};

constexpr int side_shift=3;
constexpr int change_side=1<<side_shift;
constexpr int p_limit=empty+1;
constexpr int total_phase=1<<phase_shift;
constexpr int max_ply=ply_limit-8;
constexpr double max_time_ratio=1.2;
constexpr double min_time_ratio=0.5;

constexpr int sq_file(const int sq){
  return sq&0x7;
}

constexpr u64 bb(const u8 sq){
  return 1ULL<<sq;
}

inline u8 to_white(const int piece){
  return piece&(change_side-1);
}

inline u32 set_quiet(const u32 m){
  return m|0x1000;
}

inline u32 with_promo(const u32 m,const u8 p){
  return (m&0xffff1fff)|to_white(p)<<13;
}

inline u32 with_score(const u32 m,const int s){
  return (m&0xffff)|static_cast<u32>(s)<<16;
}

inline u32 mv(const u8 from,const u8 to){
  return from<<6|to;
}

inline u32 base(const u32 m){
  return m&0xefff;
}

inline int get_side(const int piece){
  return piece>>side_shift;
}

inline u8 promoted_to(const u32 m){
  return m>>13&7;
}

inline u8 mv_from(const u32 m){
  return m>>6&0x3f;
}

inline u8 mv_to(const u32 m){
  return m&0x3f;
}

inline i16 mv_score(const u32 m){
  return static_cast<i16>(m>>16);
}

inline bool is_mate_score(const int score){
  return score<=-mate_score+max_ply||score>=mate_score-max_ply;
}

inline bool is_mv(const move_t m){
  return base(m);
}

inline bool eq(const u32 m0,const u32 m1){
  return base(m0)==base(m1);
}

inline bool is_quiet(const u32 m){
  return m&0x1000;
}

inline bool less_score(const u32 m0,const u32 m1){
  return static_cast<i32>(m0)<static_cast<i32>(m1);
}

template<typename T> constexpr T sqr(T a){
  return a*a;
}

inline int get_rank(const int sq){
  return sq>>3;
}

inline int chr_to_sq(const char f,const char r){
  return f-'a'+((7-(r-'1'))<<3);
}

inline int d_score(const int depth){
  return sqr(std::min(depth,16))*32;
}

inline int futility_margin(const int depth){
  return 80*depth;
}

inline int see_captures_margin(const int depth){
  return -100*depth;
}

inline int see_quiets_margin(const int depth){
  return -15*sqr(depth-1);
}

inline int sq_rf(const int r,const int f){
  return (r<<3)+f;
}

inline char file_chr(const int sq){
  return static_cast<char>('a' + sq_file(sq));
}

inline char rank_chr(const int sq){
  return static_cast<char>('8'-get_rank(sq));
}

template<typename T> constexpr int sign(T a){
  return (a>0)-(a<0);
}

template<typename T> constexpr T abs(T a){
  return a<0?-a:a;
}
