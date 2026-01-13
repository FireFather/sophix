#include "move.h"
#include "eval.h"
#include "hash.h"
#include "movegen.h"
#include "position.h"
#include "search.h"

namespace{
  void update_position_hash_key(position_t* pos,u64& hash_key,const int piece,const int sq){
    hash_key^=zobkeys.positions[sq][piece];
    if(equal_to(piece,pawn)||equal_to(piece,king)){
      pos->phash_key^=zobkeys.positions[sq][piece];
    }
  }

  void reset_eq_sq(position_t* pos,u64& hash_key){
    if(pos->ep_sq!=n_sqs){
      hash_key^=zobkeys.positions[pos->ep_sq][z_keys_ep_flag];
    }
    pos->ep_sq=n_sqs;
  }

  int rook_c_flag_mask[board_size];

  void set_piece(position_t* pos,u64* hash_key,const int piece,const int sq){
    const int old_piece=pos->board[sq];
    update_position_hash_key(pos,*hash_key,old_piece,sq);
    pos->board[sq]=static_cast<u8>(piece);
    update_position_hash_key(pos,*hash_key,piece,sq);
    u64 b=bb(static_cast<u8>(sq));
    if(piece==empty){
      if(old_piece!=empty){
        b=~b;
        pos->occ[get_side(old_piece)]&=b;
        pos->piece_occ[to_white(old_piece)]&=b;
      }
    } else{
      pos->occ[pos->side]|=b;
      pos->occ[pos->side^1]&=~b;
      if(equal_to(piece,king)){
        pos->k_sq[get_side(piece)]=sq;
      } else{
        pos->piece_occ[to_white(piece)]|=b;
        if(old_piece!=empty){
          pos->piece_occ[to_white(old_piece)]&=~b;
        }
      }
    }
  }

  void set_move(const search_data_t* sd,move_list_t* ml,const move_t move){
    if(is_mv(move)&&move_is_quiet(sd->pos,move)&&is_pseudo_legal(sd->pos,move)){
      ml->moves[0]=set_quiet(move);
      ml->moves_cnt=1;
    } else{
      ml->moves_cnt=0;
    }
  }

  void generate_moves(move_list_t* ml,search_data_t* sd,const int depth,const int ply){
    switch(ml->phase){
    case material_move: if(depth==0){
        checks_and_material_moves(sd->pos,ml->moves.data(),&ml->moves_cnt);
        eval_all_moves(sd,ml->moves.data(),ml->moves_cnt,ply);
      } else{
        material_moves(sd->pos,ml->moves.data(),&ml->moves_cnt,depth>0);
        eval_material_moves(sd->pos,ml->moves.data(),ml->moves_cnt);
      }
      break;
    case killer_move_0: set_move(sd,ml,sd->killer_moves[ply][0]);
      break;
    case killer_move_1: set_move(sd,ml,sd->killer_moves[ply][1]);
      break;
    case counter_move: set_move(sd,ml,get_counter_move(sd));
      break;
    case quiet_move: quiet_moves(sd->pos,ml->moves.data(),&ml->moves_cnt);
      ml->moves_cnt=eval_quiet_moves(sd,ml->moves.data(),ml->moves_cnt,ply);
      break;
    case bad_captures: ml->moves_cnt=ml->bad_captures_cnt;
      memcpy(ml->moves.data(),ml->bad_captures.data(),ml->moves_cnt*sizeof(move_t));
      break;
    case incheck: check_evasion_moves(sd->pos,ml->moves.data(),&ml->moves_cnt);
      eval_all_moves(sd,ml->moves.data(),ml->moves_cnt,ply);
      break;
    default: break;
    }
  }

  void prepare_next_move(move_t* moves,const int moves_cnt,const int i){
    int max_j=i;
    for(int j=i+1;j<moves_cnt;j++) if(less_score(moves[max_j],moves[j])) max_j=j;
    if(max_j!=i){
      const move_t tmp=moves[i];
      moves[i]=moves[max_j];
      moves[max_j]=tmp;
    }
  }
}

void make_null_move(search_data_t* sd){
  position_cpy(sd->pos+1,sd->pos);
  sd->pos++;
  reset_eq_sq(sd->pos,sd->hash_key);
  sd->pos->move=0;
  sd->pos->side^=1;
  sd->hash_key^=zobkeys.side_flag;
  sd->hash_keys[++sd->hash_keys_cnt]=sd->hash_key;
  set_pins_and_checks(sd->pos);
}

void make_move(search_data_t* sd,const move_t move){
  sd->nodes++;
  sd->pos++;
  position_t* pos=sd->pos;
  position_cpy(sd->pos,sd->pos-1);
  u64 hash_key=sd->hash_key;
  pos->move=move;
  const int from=mv_from(move);
  const int to=mv_to(move);
  const int diff=to-from;
  const int piece=pos->board[from];
  const int target_piece=pos->board[to];
  const int w_piece=to_white(piece);
  const int w_target_piece=to_white(target_piece);
  int score_mg=pst_mg[piece][to]-pst_mg[piece][from];
  int score_eg=pst_eg[piece][to]-pst_eg[piece][from];
  pos->fifty_cnt++;
  hash_key^=zobkeys.side_flag;
  if(pos->c_flag) hash_key^=zobkeys.c_flags[pos->c_flag];
  update_position_hash_key(pos,hash_key,piece,from);
  update_position_hash_key(pos,hash_key,piece,to);
  reset_eq_sq(pos,hash_key);
  pos->board[from]=empty;
  pos->board[to]=piece;
  pos->c_flag&=rook_c_flag_mask[from];
  const u64 mask=bb(from)|bb(to);
  pos->occ[pos->side]^=mask;
  if(target_piece!=empty){
    pos->fifty_cnt=0;
    const u64 bt_mask=bb(to);
    pos->occ[pos->side^1]^=bt_mask;
    if(w_target_piece==king) pos->k_sq[pos->side]=n_sqs;
    else pos->piece_occ[w_target_piece]^=bt_mask;
    score_mg+=pst_mg[target_piece][to];
    score_eg+=pst_eg[target_piece][to];
    pos->phase+=piece_phase[w_target_piece];
    update_position_hash_key(pos,hash_key,target_piece,to);
    pos->c_flag&=rook_c_flag_mask[to];
  }
  if(w_piece==king){
    pos->k_sq[pos->side]=to;
    pos->c_flag&=king_c_flag_mask[pos->side];
    if(diff==2||diff==-2){
      int corner_sq;
      const int rook_sq=(from+to)>>1;
      if(pos->side==white) corner_sq=to<from?a1:h1;
      else corner_sq=to<from?a8:h8;
      set_piece(pos,&hash_key,empty,corner_sq);
      set_piece(pos,&hash_key,f_piece(pos,rook),rook_sq);
      score_mg+=castling;
    }
  } else{
    pos->piece_occ[w_piece]^=mask;
  }
  if(pos->c_flag) hash_key^=zobkeys.c_flags[pos->c_flag];
  if(w_piece==pawn){
    pos->fifty_cnt=0;
    if(promoted_to(move)){
      const int promo=promoted_to(move);
      set_piece(pos,&hash_key,f_piece(pos,promo),to);
      score_mg+=pst_mg[f_piece(pos,promo)][to];
      score_mg-=pst_mg[f_piece(pos,pawn)][to];
      score_eg+=pst_eg[f_piece(pos,promo)][to];
      score_eg-=pst_eg[f_piece(pos,pawn)][to];
    } else{
      const int pawn_sq=to^8;
      if(diff==16||diff==-16){
        pos->ep_sq=pawn_sq;
        hash_key^=zobkeys.positions[pos->ep_sq][z_keys_ep_flag];
      } else if(target_piece==empty&&diff!=8&&diff!=-8){
        score_mg+=pst_mg[o_piece(pos,pawn)][pawn_sq];
        score_eg+=pst_eg[o_piece(pos,pawn)][pawn_sq];
        set_piece(pos,&hash_key,empty,pawn_sq);
      }
    }
  }
  if(pos->side==white){
    pos->score_mg+=score_mg;
    pos->score_eg+=score_eg;
  } else{
    pos->score_mg-=score_mg;
    pos->score_eg-=score_eg;
  }
  sd->hash_key=hash_key;
  sd->hash_keys[++sd->hash_keys_cnt]=hash_key;
  pos->side^=1;
  set_pins_and_checks(pos);
}

void make_move_rev(search_data_t* sd,const move_t move){
  make_move(sd,move);
  sd->pos--;
  position_cpy(sd->pos,sd->pos+1);
}

void init_castle(){
  constexpr int c_flag_init=c_flag_wl|c_flag_wr|
    c_flag_bl|c_flag_br;
  for(int& i:rook_c_flag_mask) i=c_flag_init;
  rook_c_flag_mask[a1]^=c_flag_wl;
  rook_c_flag_mask[h1]^=c_flag_wr;
  rook_c_flag_mask[a8]^=c_flag_bl;
  rook_c_flag_mask[h8]^=c_flag_br;
}

void init_move_list(move_list_t* ml,const int search_mode,const int in_check){
  ml->search_mode=search_mode;
  ml->moves_cnt=ml->cnt=ml->bad_captures_cnt=ml->searched_hash_move=0;
  ml->phase=in_check?incheck:material_move;
}

move_t next_move(move_list_t* ml,search_data_t* sd,const move_t hash_move,const int depth,const int ply){
  int see_score;
  if(is_mv(hash_move)&&!ml->searched_hash_move){
    ml->searched_hash_move=1;
    return hash_move;
  }
  move_t move=0;
  while(ml->phase!=end){
    if(ml->cnt==0) generate_moves(ml,sd,depth,ply);
    if(ml->cnt>=ml->moves_cnt){
      ml->cnt=0;
      if(ml->search_mode==q_search||ml->phase==incheck) ml->phase=end;
      else ml->phase++;
      continue;
    }
    if(ml->moves_cnt>1&&ml->phase!=bad_captures) prepare_next_move(ml->moves.data(),ml->moves_cnt,ml->cnt);
    const move_t next_move=ml->moves[ml->cnt++];
    if(eq(next_move,hash_move)) continue;
    if(ml->search_mode==n_search&&ml->phase==material_move&&
      (see_score=see(sd->pos,next_move,1))<0){
      ml->bad_captures[ml->bad_captures_cnt++]=
        with_score(next_move,see_score);
      continue;
    }
    move=next_move;
    break;
  }
  return move;
}
