#pragma once
#include <array>
#include <utility>

#include "bitboard.h"
#include "main.h"

struct position_t{
  move_t move;
  u8 side,ep_sq,c_flag,in_check,see_pins,fifty_cnt,phase;
  std::array<u8,n_sides> k_sq;
  std::array<u8,board_size> board;
  u64 phash_key;
  std::array<u64,n_sides> occ;
  std::array<u64,n_pieces-1> piece_occ;
  std::array<u64,n_sides> pinned;
  std::array<u64,n_sides> pinners;
  i16 score_mg,score_eg,static_score;
};

template<typename Func> void for_each_set_bit(u64 b,Func&& f){
  while(b){
    f(b&-b);
    b&=b-1;
  }
}

inline void position_cpy(position_t* dest,position_t* src){
  __m128 x0,x1,x2,x3,x4,x5,* s,* d;
  s=reinterpret_cast<__m128*>(src);
  d=reinterpret_cast<__m128*>(dest);
  x0=s[0];
  x1=s[1];
  x2=s[2];
  x3=s[3];
  x4=s[4];
  x5=s[5];
  d[0]=x0;
  d[1]=x1;
  d[2]=x2;
  d[3]=x3;
  d[4]=x4;
  d[5]=x5;
  x0=s[6];
  x1=s[7];
  x2=s[8];
  x3=s[9];
  x4=s[10];
  x5=s[11];
  d[6]=x0;
  d[7]=x1;
  d[8]=x2;
  d[9]=x3;
  d[10]=x4;
  d[11]=x5;
}

inline void pins_and_atts_to(
  const position_t* pos,const int sq,const int att_side,const int pin_side,
  u64* pinned,u64* pinners,u64* att,u64* r_att){
  const u64 occ_att=pos->occ[att_side];
  const u64 occ_pin=pos->occ[pin_side];
  const u64 occ=occ_att|pos->occ[att_side^1];
  const u64 bq=(pos->piece_occ[bishop]|pos->piece_occ[queen])&occ_att;
  const u64 rq=(pos->piece_occ[rook]|pos->piece_occ[queen])&occ_att;
  *att=bishop_att(occ,sq);
  *r_att=rook_att(occ,sq);
  u64 b=(*att^bishop_att(occ^(occ_pin&*att),sq))&bq;
  b|=(*r_att^rook_att(occ^(occ_pin&*r_att),sq))&rq;
  *pinned=*pinners=0;
  for_each_set_bit(b,[&](const u64 bit){
    const int p_sq=bsf(bit);
    if(const u64 line=bline[sq][p_sq]){
      *pinned|=line;
      *pinners|=bb(p_sq);
    }
  });
  *pinned&=occ_pin;
}

inline bool equal_to(const int piece,const int w_piece){
  return std::cmp_equal(to_white(piece),w_piece);
}

inline int move_is_quiet(const position_t* pos,const move_t move){
  if(pos->board[mv_to(move)]!=empty) return 0;
  if(equal_to(pos->board[mv_from(move)],pawn)&&
    (promoted_to(move)||mv_to(move)==pos->ep_sq))
    return 0;
  return 1;
}

inline int f_piece(const position_t* pos,const int w_piece){
  return w_piece|(pos->side<<side_shift);
}

inline int o_piece(const position_t* pos,const int w_piece){
  return w_piece|((pos->side^1)<<side_shift);
}

inline u64 occupancy(const position_t* pos){
  return pos->occ[white]|pos->occ[black];
}

void set_pins_and_checks(position_t*);
int is_pseudo_legal(const position_t* pos,move_t move);
int legal_move(const position_t*,move_t);
int see(position_t*,move_t,int);
int insuff_material(const position_t*);
int non_pawn_material(const position_t*);
void set_phase(position_t*);
void reevaluate_position(position_t*);
