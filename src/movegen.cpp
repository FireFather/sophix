#include "movegen.h"
#include "bitboard.h"
#include "main.h"
#include "position.h"

namespace{
  template<typename AttackFunc> void add_moves(
    const position_t* pos,
    const int pp,
    AttackFunc method,
    const u64 occ_f,
    u64 occ,
    u64 oc,
    move_t*& ptr
    ){
    u64 b0=pos->piece_occ[pp]&occ_f;
    while(b0){
      int from=bsf(b0);
      u64 b1=method(occ,from)&oc;
      const int mfromv=static_cast<int>(mv(static_cast<u8>(from), 0));
      while(b1){
        *ptr=mfromv|bsf(b1);
        ptr++;
        b1&=b1-1;
      }
      b0&=b0-1;
    }
  }

  template<typename AttackFunc> void count_moves(const position_t* pos,const int pp,AttackFunc method,const u64 occ_f,u64 occ,u64 n_occ_f,int& moves_cnt){
    u64 b0=pos->piece_occ[pp]&occ_f;
    for_each_set_bit(b0,[&](const u64 bit){
      moves_cnt+=popcnt(method(occ,bsf(bit))&n_occ_f);
    });
  }

  template<typename AttackFunc> void add_captures_and_checks(
    AttackFunc method,
    u64 b0,
    u64 occ,
    const u64 n_occ_f,
    const int k_sq,
    move_t*& ptr
    ){
    for_each_set_bit(b0,[&](const u64 bit0){
      int from=bsf(bit0);
      u64 b1=method(occ,from)&(n_occ_f&~bline[k_sq][from]);
      from=static_cast<int>(mv(static_cast<u8>(from), 0));
      for_each_set_bit(b1,[&](const u64 bit1){
        *ptr=from|bsf(bit1);
        ++ptr;
      });
    });
  }

  void add_king_moves(const position_t* pos,const u64 oc,move_t*& ptr){
    int from=pos->k_sq[pos->side];
    const u64 b0=piece_area[king][from]&oc;
    from=static_cast<int>(mv(from, 0));
    for_each_set_bit(b0,[&](const u64 bit){
      *ptr=from|bsf(bit);
      ++ptr;
    });
  }

  void add_promotion(const int from,const int to,const int piece,move_t*& ptr){
    *ptr=with_promo(
      with_score(mv(from,to),-p_limit+piece),
      piece
      );
    ptr++;
  }

  void add_move(const int from,const int to,move_t*& ptr){
    *ptr=mv(static_cast<u8>(from),static_cast<u8>(to));
    ptr++;
  }

  void add_quiet_pawn_moves(const position_t* pos,move_t** moves,
    const u64 occ_f,const u64 n_occ,const u64 occ_x){
    int to;
    u64 b,p_sh_occ;
    move_t* ptr=*moves;
    const u64 p_occ=pos->piece_occ[pawn]&occ_f;
    if(pos->side==white){
      p_sh_occ=(p_occ&~rank7)>>8&n_occ;
      b=p_sh_occ&occ_x;
      for_each_set_bit(b,[&](const u64 bit){
        to=bsf(bit);
        add_move(to+8,to,ptr);
      });
      b=p_sh_occ>>8&n_occ&rank4&occ_x;
      for_each_set_bit(b,[&](const u64 bit){
        to=bsf(bit);
        add_move(to+16,to,ptr);
      });
    } else{
      p_sh_occ=(p_occ&~rank2)<<8&n_occ;
      b=p_sh_occ&occ_x;
      for_each_set_bit(b,[&](const u64 bit){
        to=bsf(bit);
        add_move(to-8,to,ptr);
      });
      b=p_sh_occ<<8&n_occ&rank5&occ_x;
      for_each_set_bit(b,[&](const u64 bit){
        to=bsf(bit);
        add_move(to-16,to,ptr);
      });
    }
    *moves=ptr;
  }

  void add_pawn_captures(
    const position_t* pos,move_t** moves,const u64 occ_f,const u64 occ_o,
    const int minor_promotions){
    int to;
    u64 b1;
    move_t* ptr=*moves;
    const u64 p_occ=pos->piece_occ[pawn]&occ_f;
    u64 b0=pawn_atts(p_occ,pos->side);
    u64 p_occ_c=occ_o;
    if(pos->ep_sq!=n_sqs) p_occ_c|=bb(pos->ep_sq);
    b0&=p_occ_c;
    const int piece=o_piece(pos,pawn);
    const u64 bp=b0&(pos->side==white?rank8:rank1);
    b0^=bp;
    for_each_set_bit(b0,[&](const u64 bit0){
      to=bsf(bit0);
      b1=piece_area[piece][to]&p_occ;
      for_each_set_bit(b1,[&](const u64 bit1){
        add_move(bsf(bit1),to,ptr);
      });
    });
    for_each_set_bit(bp,[&](const u64 bitp){
      to=bsf(bitp);
      b1=piece_area[piece][to]&p_occ;
      for_each_set_bit(b1,[&](const u64 bit1){
        const int from=bsf(bit1);
        add_promotion(from,to,queen,ptr);
        if(minor_promotions){
          add_promotion(from,to,rook,ptr);
          add_promotion(from,to,bishop,ptr);
          add_promotion(from,to,knight,ptr);
        }
      });
    });
    *moves=ptr;
  }

  void add_non_capture_promotions(
    const position_t* pos,move_t** moves,const u64 occ_f,const u64 n_occ,const u64 occ_x,const int minor_promotions){
    u64 b;
    move_t* ptr=*moves;
    const u64 p_occ=pos->piece_occ[pawn]&occ_f;
    if(pos->side==white) b=(p_occ&rank7)>>8&n_occ&occ_x;
    else b=(p_occ&rank2)<<8&n_occ&occ_x;
    for_each_set_bit(b,[&](const u64 bit){
      const int to=bsf(bit);
      const int from=to^8;
      add_promotion(from,to,queen,ptr);
      if(minor_promotions){
        add_promotion(from,to,rook,ptr);
        add_promotion(from,to,bishop,ptr);
        add_promotion(from,to,knight,ptr);
      }
    });
    *moves=ptr;
  }

  void add_castling_moves(const position_t* pos,move_t** moves){
    move_t* ptr=*moves;
    if(pos->side==white){
      if(pos->c_flag&c_flag_wr&&pos->board[f1]==empty&&pos->board[g1]==empty) add_move(e1,g1,ptr);
      if(pos->c_flag&c_flag_wl&&
        pos->board[b1]==empty&&pos->board[c1]==empty&&pos->board[d1]==empty)
        add_move(e1,c1,ptr);
    } else if(pos->side==black){
      if(pos->c_flag&c_flag_br&&pos->board[f8]==empty&&pos->board[g8]==empty) add_move(e8,g8,ptr);
      if(pos->c_flag&c_flag_bl&&
        pos->board[b8]==empty&&pos->board[c8]==empty&&pos->board[d8]==empty)
        add_move(e8,c8,ptr);
    }
    *moves=ptr;
  }
}

void material_moves(const position_t* pos,move_t* moves,int* moves_cnt,const int minor_promotions){
  move_t* ptr;
  ptr=moves;
  const u64 occ_f=pos->occ[pos->side];
  const u64 occ_o=pos->occ[pos->side^1];
  const u64 occ=occ_f|occ_o;
  const u64 n_occ=~occ;
  add_king_moves(pos,occ_o,ptr);
  add_pawn_captures(pos,&ptr,occ_f,occ_o,minor_promotions);
  add_moves(pos,knight,knight_att,occ_f,occ,occ_o,ptr);
  add_moves(pos,bishop,bishop_att,occ_f,occ,occ_o,ptr);
  add_moves(pos,rook,rook_att,occ_f,occ,occ_o,ptr);
  add_moves(pos,queen,queen_att,occ_f,occ,occ_o,ptr);
  add_non_capture_promotions(pos,&ptr,occ_f,n_occ,n_occ,minor_promotions);
  *moves_cnt=static_cast<int>(ptr - moves);
}

void quiet_moves(const position_t* pos,move_t* moves,int* moves_cnt){
  move_t* ptr;
  ptr=moves;
  const u64 occ_f=pos->occ[pos->side];
  const u64 occ=occ_f|pos->occ[pos->side^1];
  const u64 n_occ=~occ;
  if(pos->c_flag) add_castling_moves(pos,&ptr);
  add_king_moves(pos,n_occ,ptr);
  add_quiet_pawn_moves(pos,&ptr,occ_f,n_occ,n_occ);
  add_moves(pos,knight,knight_att,occ_f,occ,n_occ,ptr);
  add_moves(pos,bishop,bishop_att,occ_f,occ,n_occ,ptr);
  add_moves(pos,rook,rook_att,occ_f,occ,n_occ,ptr);
  add_moves(pos,queen,queen_att,occ_f,occ,n_occ,ptr);
  *moves_cnt=static_cast<int>(ptr - moves);
}

void get_all_moves(const position_t* pos,move_t* moves,int* moves_cnt){
  int quiet_moves_cnt;
  material_moves(pos,moves,moves_cnt,1);
  quiet_moves(pos,moves+*moves_cnt,&quiet_moves_cnt);
  *moves_cnt+=quiet_moves_cnt;
}

void check_evasion_moves(const position_t* pos,move_t* moves,int* moves_cnt){
  move_t* ptr;
  ptr=moves;
  const u64 occ_f=pos->occ[pos->side];
  const u64 occ_o=pos->occ[pos->side^1];
  const u64 occ=occ_f|occ_o;
  const u64 n_occ=~occ;
  const int k_sq=pos->k_sq[pos->side];
  add_king_moves(pos,~occ_f,ptr);
  u64 att=piece_area[f_piece(pos,pawn)][k_sq]&pos->piece_occ[pawn];
  att|=piece_area[knight][k_sq]&pos->piece_occ[knight];
  att|=bishop_att(occ,k_sq)&(pos->piece_occ[bishop]|pos->piece_occ[queen]);
  att|=rook_att(occ,k_sq)&(pos->piece_occ[rook]|pos->piece_occ[queen]);
  att&=occ_o;
  if(popcnt(att)==1){
    const int att_sq=bsf(att);
    const u64 att_line=bline[att_sq][k_sq];
    const u64 occ_att=att_line|att;
    add_pawn_captures(pos,&ptr,occ_f,att,1);
    add_non_capture_promotions(pos,&ptr,occ_f,n_occ,att_line,1);
    add_quiet_pawn_moves(pos,&ptr,occ_f,n_occ,occ_att);
    add_moves(pos,knight,knight_att,occ_f,occ,occ_att,ptr);
    add_moves(pos,bishop,bishop_att,occ_f,occ,occ_att,ptr);
    add_moves(pos,rook,rook_att,occ_f,occ,occ_att,ptr);
    add_moves(pos,queen,queen_att,occ_f,occ,occ_att,ptr);
  }
  *moves_cnt=static_cast<int>(ptr - moves);
}

void king_moves(const position_t* pos,move_t* moves,int* moves_cnt){
  move_t* ptr;
  ptr=moves;
  const u64 occ_f=pos->occ[pos->side];
  const u64 n_occ_f=~occ_f;
  if(pos->c_flag) add_castling_moves(pos,&ptr);
  add_king_moves(pos,n_occ_f,ptr);
  *moves_cnt=static_cast<int>(ptr - moves);
}

void checks_and_material_moves(const position_t* pos,move_t* moves,int* moves_cnt){
  u64 att,r_att,pinned,pinners;
  move_t* ptr;
  if(pos->in_check) return;
  ptr=moves;
  u64 occ_f=pos->occ[pos->side];
  const u64 occ_o=pos->occ[pos->side^1];
  const u64 occ=occ_f|occ_o;
  const u64 n_occ_f=~occ_f;
  const u64 n_occ=~occ;
  const int k_sq=pos->k_sq[pos->side^1];
  const u64 n_att=knight_att(occ,k_sq);
  pins_and_atts_to(pos,k_sq,pos->side,pos->side,
    &pinned,&pinners,&att,&r_att);
  add_moves(pos,queen,queen_att,occ_f,occ,occ_o|(n_occ&(r_att|att)),ptr);
  occ_f&=pinned;
  if(occ_f){
    add_moves(pos,knight,knight_att,occ_f,occ,n_occ_f,ptr);
    const u64 p_occ=pos->piece_occ[pawn]&occ_f;
    for_each_set_bit(p_occ,[&](const u64 bit){
      const int from=bsf(bit);
      if(const u64 oc=n_occ&~bline[k_sq][from]){
        add_pawn_captures(pos,&ptr,bb(from),occ_o,1);
        add_quiet_pawn_moves(pos,&ptr,bb(from),n_occ,oc);
        add_non_capture_promotions(pos,&ptr,bb(from),n_occ,oc,1);
      }
    });
    u64 b0=bb(pos->k_sq[pos->side]);
    if(b0&occ_f) add_captures_and_checks(king_att,b0,occ,n_occ_f,k_sq,ptr);
    b0=pos->piece_occ[bishop]&occ_f;
    add_captures_and_checks(bishop_att,b0,occ,n_occ_f,k_sq,ptr);
    b0=pos->piece_occ[rook]&occ_f;
    add_captures_and_checks(rook_att,b0,occ,n_occ_f,k_sq,ptr);
    b0=pos->piece_occ[queen]&occ_f;
    add_captures_and_checks(queen_att,b0,occ,n_occ_f,k_sq,ptr);
  }
  occ_f=pos->occ[pos->side]&~pinned;
  add_king_moves(pos,occ_o,ptr);
  add_pawn_captures(pos,&ptr,occ_f,occ_o,1);
  add_quiet_pawn_moves(pos,&ptr,occ_f,n_occ,piece_area[o_piece(pos,pawn)][k_sq]);
  add_non_capture_promotions(pos,&ptr,occ_f,n_occ,n_occ,1);
  add_moves(pos,knight,knight_att,occ_f,occ,occ_o|(n_occ&n_att),ptr);
  add_moves(pos,bishop,bishop_att,occ_f,occ,occ_o|(n_occ&att),ptr);
  add_moves(pos,rook,rook_att,occ_f,occ,occ_o|(n_occ&r_att),ptr);
  *moves_cnt=static_cast<int>(ptr - moves);
}

int non_king_moves(const position_t* pos){
  int moves_cnt=0;
  const u64 occ_f=pos->occ[pos->side];
  const u64 occ_o=pos->occ[pos->side^1];
  const u64 occ=occ_f|occ_o;
  const u64 n_occ_f=~occ_f;
  const u64 n_occ=~occ;
  count_moves(pos,knight,knight_att,occ_f,occ,n_occ_f,moves_cnt);
  count_moves(pos,queen,queen_att,occ_f,occ,n_occ_f,moves_cnt);
  count_moves(pos,bishop,bishop_att,occ_f,occ,n_occ_f,moves_cnt);
  count_moves(pos,rook,rook_att,occ_f,occ,n_occ_f,moves_cnt);
  const u64 p_occ=pos->piece_occ[pawn]&occ_f;
  u64 b0=pawn_atts(p_occ,pos->side);
  u64 p_occ_c=occ_o;
  if(pos->ep_sq!=n_sqs) p_occ_c|=bb(pos->ep_sq);
  b0&=p_occ_c;
  const int piece=o_piece(pos,pawn);
  const u64 b1=b0&(pos->side==white?rank8:rank1);
  b0^=b1;
  for_each_set_bit(b0,[&](const u64 bit){
    moves_cnt+=popcnt(piece_area[piece][bsf(bit)]&p_occ);
  });
  for_each_set_bit(b1,[&](const u64 bit){
    moves_cnt+=popcnt(piece_area[piece][bsf(bit)]&p_occ)<<2;
  });
  u64 p_sh_occ=p_occ;
  if(pos->side==white){
    p_sh_occ=p_sh_occ>>8&n_occ;
    moves_cnt+=popcnt(p_sh_occ&~rank8);
    moves_cnt+=popcnt(p_sh_occ&rank8)<<2;
    p_sh_occ=p_sh_occ>>8&n_occ&rank4;
    moves_cnt+=popcnt(p_sh_occ);
  } else{
    p_sh_occ=p_sh_occ<<8&n_occ;
    moves_cnt+=popcnt(p_sh_occ&~rank1);
    moves_cnt+=popcnt(p_sh_occ&rank1)<<2;
    p_sh_occ=p_sh_occ<<8&n_occ&rank5;
    moves_cnt+=popcnt(p_sh_occ);
  }
  return moves_cnt;
}
