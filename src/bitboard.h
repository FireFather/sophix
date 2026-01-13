#pragma once
#include <array>
#include <intrin.h>
#include "main.h"

inline constexpr auto rank1=0xff00000000000000ULL;
inline constexpr auto rank2=0x00ff000000000000ULL;
inline constexpr auto rank4=0x000000ff00000000ULL;
inline constexpr auto rank5=0x00000000ff000000ULL;
inline constexpr auto rank7=0x000000000000ff00ULL;
inline constexpr auto rank8=0x00000000000000ffULL;
inline constexpr auto filea=0x0101010101010101ULL;
inline constexpr auto fileh=0x8080808080808080ULL;
inline constexpr auto q_side=0x0f0f0f0f0f0f0f0fULL;
inline constexpr auto k_side=0xf0f0f0f0f0f0f0f0ULL;
inline constexpr auto ring=0x007e7e7e7e7e7e00ULL;
inline constexpr auto ring_c=0x817e7e7e7e7e7e81ULL;

inline constexpr u64 k1=0x5555555555555555;
inline constexpr u64 k2=0x3333333333333333;
inline constexpr u64 k4=0x0f0f0f0f0f0f0f0f;
inline constexpr u64 kf=0x0101010101010101;

inline int popcnt(u64 b){
  b=b-(b>>1&k1);
  b=(b&k2)+(b>>2&k2);
  b=(b+(b>>4))&k4;
  b=b*kf>>56;
  return SC<int>(b);
}

inline const int debruin[64]={
0,47,1,56,48,27,2,60,
57,49,41,37,28,16,3,61,
54,58,35,52,50,42,21,44,
38,32,29,23,17,11,4,62,
46,55,26,59,40,36,15,53,
34,51,20,43,31,22,10,45,
25,39,14,33,19,30,9,24,
13,18,8,12,7,6,5,63
};

inline int bsf(const u64 b){
  return debruin[0x03f79d71b4cb0a89*(b^(b-1))>>58];
}

inline u64 pext(const u64 occ,const u64 mask){
  return _pext_u64(occ,mask);
}

struct att_lookup_t{
  u64* attacks;
  u64 mask;
};

extern std::array<att_lookup_t,board_size> bishop_att_lookup;
extern std::array<att_lookup_t,board_size> rook_att_lookup;
extern std::array<std::array<u64,board_size>,p_limit> piece_area;
extern std::array<std::array<u64,board_size>,n_sides> passer_area;
extern std::array<std::array<u64,board_size>,n_sides> connected_pawn_area;
extern std::array<std::array<u64,board_size>,n_sides> doubled_pawn_area;
extern std::array<u64,n_files> isolated_pawn_area;
extern std::array<u64,n_files> file;
extern std::array<u64,board_size> king_zone;
extern std::array<std::array<u64,board_size>,board_size> bline;

inline u64 knight_att(u64 occ,const int sq){
  return piece_area[knight][sq];
}

inline u64 bishop_att(const u64 occ,const int sq){
  const att_lookup_t* att=&bishop_att_lookup[sq];
  return att->attacks[pext(occ,att->mask)];
}

inline u64 rook_att(const u64 occ,const int sq){
  const att_lookup_t* att=&rook_att_lookup[sq];
  return att->attacks[pext(occ,att->mask)];
}

inline u64 queen_att(const u64 occ,const int sq){
  return bishop_att(occ,sq)|rook_att(occ,sq);
}

inline u64 king_att(u64 occ,const int sq){
  return piece_area[king][sq];
}

inline u64 pawn_atts(const u64 p_occ,const int side){
  if(side==white) return ((p_occ>>7)&~filea)|((p_occ>>9)&~fileh);
  return ((p_occ<<9)&~filea)|((p_occ<<7)&~fileh);
}

inline u64 pushed_pawns(const u64 p_occ,const u64 n_occ,const int side){
  u64 b;
  if(side==white){
    b=p_occ>>8&n_occ;
    return b|((b>>8)&n_occ&rank4);
  }
  b=p_occ<<8&n_occ;
  return b|((b<<8)&n_occ&rank5);
}

void init_bbs();
void init();
