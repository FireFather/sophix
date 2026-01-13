#include "position.h"
#include <algorithm>
#include <utility>
#include "bitboard.h"
#include "eval.h"

void pins_and_atts_to(const position_t* pos, const int sq, const int att_side, const int pin_side, u64* pinned,
  u64* pinners, u64* att, u64* r_att){
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
      *pinners|=bb(static_cast<u8>(p_sq));
    }
  });
  *pinned&=occ_pin;
}
void set_pins_and_checks(position_t* pos){
  u64 att,r_att;
  const int k_sq=pos->k_sq[pos->side];
  if(k_sq==n_sqs) return;
  const int side=pos->side;
  pins_and_atts_to(pos,pos->k_sq[side],side^1,side,
    &pos->pinned[side],&pos->pinners[side^1],
    &att,&r_att);
  const u64 occ_o=pos->occ[side^1];
  const u64 bq=(pos->piece_occ[bishop]|pos->piece_occ[queen])&occ_o;
  const u64 rq=(pos->piece_occ[rook]|pos->piece_occ[queen])&occ_o;
  int in_check=att&bq||r_att&rq?1:0;
  if(!in_check){
    if(piece_area[f_piece(pos,pawn)][k_sq]&pos->piece_occ[pawn]&occ_o) in_check=1;
    else if(piece_area[knight][k_sq]&pos->piece_occ[knight]&occ_o) in_check=1;
    else if(piece_area[king][k_sq]&bb(pos->k_sq[pos->side^1])) in_check=1;
  }
  pos->in_check=in_check;
  pos->see_pins=0;
}

namespace{
  int attacked(const position_t* pos,const int sq){
    const u64 occ_o=pos->occ[pos->side^1];
    if(piece_area[f_piece(pos,pawn)][sq]&pos->piece_occ[pawn]&occ_o) return 1;
    if(piece_area[knight][sq]&pos->piece_occ[knight]&occ_o) return 1;
    if(piece_area[king][sq]&bb(pos->k_sq[pos->side^1])) return 1;
    const u64 full_occ=pos->occ[pos->side]|occ_o;
    u64 acc=(pos->piece_occ[bishop]|pos->piece_occ[queen])&occ_o;
    if(bishop_att(full_occ,sq)&acc) return 1;
    acc=(pos->piece_occ[rook]|pos->piece_occ[queen])&occ_o;
    if(rook_att(full_occ,sq)&acc) return 1;
    return 0;
  }

  int attacked_after_move(const position_t* pos,const int sq,const move_t move){
    const int from=mv_from(move);
    const int to=mv_to(move);
    const int w_piece=to_white(pos->board[from]);
    const u64 fb=bb(from);
    const u64 tb=bb(to);
    u64 occ_o=pos->occ[pos->side^1];
    if(pos->board[to]!=empty) occ_o^=tb;
    else if(w_piece==pawn&&std::cmp_equal(pos->ep_sq,to)) occ_o^=bb(to^8);
    if(piece_area[f_piece(pos,pawn)][sq]&pos->piece_occ[pawn]&occ_o) return 1;
    if(piece_area[knight][sq]&pos->piece_occ[knight]&occ_o) return 1;
    if(piece_area[king][sq]&bb(pos->k_sq[pos->side^1])) return 1;
    const u64 occ_f=(pos->occ[pos->side]^fb)|tb;
    const u64 occ=occ_f|occ_o;
    u64 occ_x=(pos->piece_occ[bishop]|pos->piece_occ[queen])&occ_o;
    if(bishop_att(occ,sq)&occ_x) return 1;
    occ_x=(pos->piece_occ[rook]|pos->piece_occ[queen])&occ_o;
    if(rook_att(occ,sq)&occ_x) return 1;
    return 0;
  }
}

int is_pseudo_legal(const position_t* pos,const move_t move){
  const u8 to=mv_to(move);
  const u8 from=mv_from(move);
  const int diff=to-from;
  const int t_piece=pos->board[to];
  int piece=pos->board[from];
  if(piece==empty||get_side(piece)!=pos->side) return 0;
  if(t_piece!=empty&&get_side(piece)==get_side(t_piece)) return 0;
  piece=to_white(piece);
  if(piece==pawn){
    if(!promoted_to(move)&&(get_rank(to)==rank_8||get_rank(to)==rank_1)) return 0;
    const u64 occ=occupancy(pos);
    u64 p_area=pushed_pawns(bb(from),~occ,pos->side)&bb(to);
    if(p_area) return 1;
    p_area=pawn_atts(bb(from),pos->side)&bb(to);
    if(p_area){
      if(t_piece!=empty) return 1;
      if(std::cmp_equal(to,pos->ep_sq)) return 1;
    }
    return 0;
  }
  if(piece!=king){
    if(!(piece_area[piece][from]&bb(to))) return 0;
    if(piece==knight) return 1;
    return (bline[from][to]&occupancy(pos))==0;
  }
  if(diff==2||diff==-2){
    if(!pos->c_flag||pos->board[to]!=empty||
      pos->board[(from+to)>>1]!=empty)
      return 0;
    if(diff>0) return pos->c_flag&(pos->side==white?c_flag_wr:c_flag_br);
    return pos->board[to-1]==empty&&
      (pos->c_flag&(pos->side==white?c_flag_wl:c_flag_bl));
  }
  return !!(piece_area[king][from]&bb(to));
}

int legal_move(const position_t* pos,const move_t move){
  const int k_sq=pos->k_sq[pos->side];
  if(k_sq==n_sqs) return 0;
  const int from=mv_from(move);
  const int to=mv_to(move);
  const u64 pinned=pos->pinned[pos->side];
  const int w_piece=to_white(pos->board[from]);
  if(pinned==0&&w_piece!=king&&!pos->in_check&&
    (std::cmp_not_equal(pos->ep_sq,to)||w_piece!=pawn))
    return 1;
  if(w_piece==king){
    if(const int diff=to-from;(diff==2||diff==-2)&&
      (pos->in_check||attacked(pos,(mv_from(move)+mv_to(move))>>1)))
      return 0;
    return !attacked_after_move(pos,to,move);
  }
  if(pos->in_check||(std::cmp_equal(pos->ep_sq,to)&&w_piece==pawn)) return !attacked_after_move(pos,k_sq,move);
  if((pinned&bb(from))==0) return 1;
  return bline[k_sq][to]&bb(from)||bline[k_sq][from]&bb(to);
}

int see(position_t* pos,const move_t move,const int prune_positive){
  int gain[max_captures];
  u64 pb=0,b_att,r_att;
  const int sq=mv_to(move);
  const int from=mv_from(move);
  int p=pos->board[from];
  const int captured=pos->board[sq];
  int side=get_side(p);
  p=to_white(p);
  int pcval=piece_value[p];
  int captured_value=0;
  if(captured!=empty){
    captured_value=piece_value[to_white(captured)];
    if(prune_positive&&pcval<=captured_value) return 0;
  }
  const int is_promotion=promoted_to(move);
  const int pqv=piece_value[queen]-piece_value[pawn];
  u64 occ=occupancy(pos)^1ULL<<from;
  gain[0]=captured_value;
  if(is_promotion&&p==pawn){
    pcval+=pqv;
    gain[0]+=pqv;
  } else if(std::cmp_equal(sq,pos->ep_sq)&&p==pawn){
    occ^=1ULL<<(sq^8);
    gain[0]=piece_value[pawn];
  }
  const u64 bq=pos->piece_occ[bishop]|pos->piece_occ[queen];
  const u64 rq=pos->piece_occ[rook]|pos->piece_occ[queen];
  u64 att=piece_area[pawn|change_side][sq]&pos->piece_occ[pawn]&pos->occ[white];
  att|=piece_area[pawn][sq]&pos->piece_occ[pawn]&pos->occ[black];
  att|=king_att(occ,sq)&(bb(pos->k_sq[white])|bb(pos->k_sq[black]));
  att|=knight_att(occ,sq)&pos->piece_occ[knight];
  att|=bishop_att(occ,sq)&bq;
  att|=rook_att(occ,sq)&rq;
  att&=occ;
  if(att&&!pos->see_pins){
    pos->see_pins=1;
    pins_and_atts_to(pos,pos->k_sq[pos->side^1],pos->side,pos->side^1,
      &pos->pinned[pos->side^1],&pos->pinners[pos->side],
      &b_att,&r_att);
  }
  int cnt=1;
  while(att){
    side^=1;
    u64 side_att=att&pos->occ[side];
    if(!(pos->pinners[side^1]&~occ)) side_att&=~pos->pinned[side];
    if(side_att==0) break;
    for(p=pawn;p<=queen;p++) if((pb=side_att&pos->piece_occ[p])) break;
    if(!pb) pb=side_att;
    pb=pb&-pb;
    occ^=pb;
    if(p==pawn||p==bishop||p==queen) att|=bishop_att(occ,sq)&bq;
    if(p==rook||p==queen) att|=rook_att(occ,sq)&rq;
    att&=occ;
    gain[cnt]=pcval-gain[cnt-1];
    pcval=piece_value[p];
    if(is_promotion&&p==pawn){
      pcval+=pqv;
      gain[cnt]+=pqv;
    }
    cnt++;
  }
  while(--cnt) gain[cnt-1]=std::min(gain[cnt-1],-gain[cnt]);
  return gain[0];
}

int insuff_material(const position_t* pos){
  const u64 occ=occupancy(pos);
  return popcnt(occ)==3&&
    occ&(pos->piece_occ[knight]|pos->piece_occ[bishop]);
}

int non_pawn_material(const position_t* pos){
  const u64 occ_f=pos->occ[pos->side];
  if((pos->piece_occ[queen]|pos->piece_occ[rook])&occ_f) return 2;
  if((pos->piece_occ[bishop]|pos->piece_occ[knight])&occ_f) return 1;
  return 0;
}

void set_phase(position_t* pos){
  int phase=16*piece_phase[pawn]+4*piece_phase[knight]+
    4*piece_phase[bishop]+4*piece_phase[rook]+
    2*piece_phase[queen];
  for(const int piece:pos->board){
    if(piece!=empty) phase-=piece_phase[to_white(piece)];
  }
  pos->phase=std::max(phase,0);
}

void reevaluate_position(position_t* pos){
  int score_mg[n_sides];
  int score_eg[n_sides];
  score_mg[white]=score_mg[black]=0;
  score_eg[white]=score_eg[black]=0;
  for(int i=0;i<board_size;i++){
    if(const int piece=pos->board[i];piece!=empty){
      score_mg[get_side(piece)]+=pst_mg[piece][i];
      score_eg[get_side(piece)]+=pst_eg[piece][i];
    }
  }
  pos->score_mg=score_mg[white]-score_mg[black];
  pos->score_eg=score_eg[white]-score_eg[black];
}
