#include "search.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include "bitboard.h"
#include "eval.h"
#include "hash.h"
#include "main.h"
#include "movegen.h"
#include "position.h"
#include "uci.h"
#include "util.h"

search_t search_config;
search_state_t search_state;

int lmr[max_depth][max_moves];
std::atomic<int> search_depth_cnt[max_depth];
move_t pv[ply_limit * ply_limit];

void init_lmr(){
  for(int d=0;d<max_depth;d++) for(int m=0;m<max_moves;m++) lmr[d][m]=static_cast<int>(1.0 + std::log(d) * std::log(m) * 0.5);
}

namespace{
  int draw(const search_data_t* sd){
    const int fifty_cnt=sd->pos->fifty_cnt;
    if(fifty_cnt>=100||insuff_material(sd->pos)) return 1;
    if(fifty_cnt<4) return 0;
    for(int i=sd->hash_keys_cnt-2;i>=sd->hash_keys_cnt-fifty_cnt;i-=2) if(sd->hash_keys[i]==sd->hash_key) return 1;
    return 0;
  }

  void update_pv(const move_t move,const int ply){
    move_t* dest=pv+ply*ply_limit;
    const move_t* src=dest+ply_limit;
    *dest++=move;
    while((*dest++=*src++));
  }

  int qsearch(search_data_t* sd,const int pv_node,int alpha,int beta,const int depth,const int ply){
    position_t* pos=sd->pos;
    move_list_t move_list;
    alpha=std::max(alpha,-mate_score+ply);
    beta=std::min(beta,mate_score-ply+1);
    if(alpha>=beta) return alpha;
    if(ply>=max_ply) return eval(pos);
    if(draw(sd)) return 0;
    const hash_data_t hash_data=get_hash(sd);
    move_t hash_move=0;
    int hash_score=-mate_score;
    int hash_bound=hash_bound_unused;
    const int hash_depth=(pos->in_check||depth==0)?0:-1;
    if(hash_data.raw){
      hash_move=hash_data.move;
      hash_bound=hash_data.bound;
      hash_score=adjust_hash(mv_score(hash_move),ply);
      if(!pv_node&&hash_data.depth>=hash_depth){
        if((hash_bound==hash_lower&&hash_score>=beta)||
          (hash_bound==hash_upper&&hash_score<=alpha)||
          hash_bound==hash_exact){
          return hash_score;
        }
      }
    }
    int static_score;
    int best_score;
    if(pos->in_check){
      static_score=best_score=-mate_score+ply;
    } else{
      static_score=best_score=hash_data.raw?hash_data.static_eval:eval(pos);
      if(hash_data.raw){
        if((hash_bound==hash_lower&&hash_score>static_score)||
          (hash_bound==hash_upper&&hash_score<static_score)||
          hash_bound==hash_exact){
          best_score=hash_score;
        }
      }
      if(best_score>=beta) return best_score;
      alpha=std::max(alpha,best_score);
    }
    move_t best_move=hash_move;
    hash_bound=hash_upper;
    init_move_list(&move_list,q_search,pos->in_check);
    move_t move;
    while((move=next_move(&move_list,sd,hash_move,depth,ply))){
      if(!pos->in_check&&see(pos,move,1)<0) continue;
      if(!legal_move(pos,move)) continue;
      make_move(sd,move);
      const int score=-qsearch(sd,0,-beta,-alpha,depth-1,ply+1);
      undo_move(sd);
      if(score>best_score){
        best_score=score;
        if(score>alpha){
          best_move=move;
          alpha=score;
          hash_bound=hash_exact;
          if(alpha>=beta){
            hash_bound=hash_lower;
            break;
          }
        }
      }
    }
    set_hash(sd,best_move,best_score,static_score,hash_depth,ply,hash_bound);
    return best_score;
  }

  int pvs(
    search_data_t* sd,
    int root_node,
    int pv_node,
    int alpha,
    int beta,
    int depth,
    int ply,
    int use_pruning,
    move_t skip_move){
    position_t* pos=sd->pos;
    move_list_t move_list;
    move_t quiet_moves[max_moves];
    i16* cmh_ptr[max_cmh_ply]={};
    if(depth<=0) return qsearch(sd,pv_node,alpha,beta,0,ply);
    alpha=std::max(alpha,-mate_score+ply);
    beta=std::min(beta,mate_score-ply+1);
    if(alpha>=beta) return alpha;
    if(ply>=max_ply) return eval(pos);
    if(search_state.done) return 0;
    if(!root_node&&draw(sd)) return 0;
    set_counter_move_history_pointer(cmh_ptr,sd,ply);
    int use_hash=!skip_move;
    move_t hash_move=0;
    int hash_score=-mate_score;
    int hash_bound=hash_bound_unused;
    hash_data_t hash_data={};
    if(use_hash){
      hash_data=get_hash(sd);
      if(hash_data.raw){
        hash_move=hash_data.move;
        hash_bound=hash_data.bound;
        hash_score=adjust_hash(mv_score(hash_move),ply);
        if(!pv_node&&hash_data.depth>=depth){
          if((hash_bound==hash_lower&&hash_score>=beta)||
            (hash_bound==hash_upper&&hash_score<=alpha)||
            hash_bound==hash_exact){
            if(is_quiet(hash_move)){
              if(hash_bound==hash_lower){
                set_killer_move(sd,hash_move,ply);
                set_counter_move(sd,hash_move);
                add_to_history(sd,cmh_ptr,hash_move,d_score(depth));
              } else if(hash_bound==hash_upper){
                add_to_history(sd,cmh_ptr,hash_move,-d_score(depth));
              }
            }
            return hash_score;
          }
        }
      }
    }
    int static_score=0;
    if(pos->in_check) static_score=-mate_score+ply;
    else if(hash_data.raw) static_score=hash_data.static_eval;
    else{
      static_score=eval(pos);
      if(use_hash) set_hash(sd,0,0,static_score,min_hash_depth,ply,hash_bound_unused);
    }
    pos->static_score=static_score;
    int improving=!pos->in_check&&ply>=2&&static_score>=(sd->pos-2)->static_score;
    int best_score=static_score;
    if(hash_data.raw&&!pos->in_check){
      if((hash_bound==hash_lower&&hash_score>static_score)||
        (hash_bound==hash_upper&&hash_score<static_score)||
        hash_bound==hash_exact)
        best_score=hash_score;
    }
    if(use_pruning&&!pos->in_check&&!is_mate_score(beta)){
      if(!pv_node){
        if(depth<=razor_depth&&best_score+razor_margin<beta){
          if(int score=qsearch(sd,0,alpha,beta,0,ply);score<beta) return score;
        }
        if(non_pawn_material(pos)){
          if(depth<=futility_depth&&best_score>=beta+futility_margin(depth)) return best_score;
          if(depth>=2&&best_score>=beta){
            int reduction=depth/4+3+std::min((best_score-beta)/80,3);
            make_null_move(sd);
            int score=-pvs(sd,0,0,-beta,-beta+1,depth-reduction,ply+1,0,0);
            undo_move(sd);
            if(search_state.done) return 0;
            if(score>=beta) return is_mate_score(score)?beta:score;
          }
        }
        if(depth>=probcut_depth){
          int beta_cut=beta+probcut_margin;
          init_move_list(&move_list,q_search,pos->in_check);
          move_t move;
          while((move=next_move(&move_list,sd,hash_move,depth,ply))){
            if(is_quiet(move)||see(pos,move,0)<beta_cut-static_score) continue;
            if(!legal_move(pos,move)) continue;
            make_move(sd,move);
            int score=-qsearch(sd,0,-beta_cut,-beta_cut+1,0,ply);
            if(score>=beta_cut) score=-pvs(sd,0,0,-beta_cut,-beta_cut+1,depth-probcut_depth+1,ply+1,1,0);
            undo_move(sd);
            if(score>=beta_cut) return score;
          }
        }
      }
      if(depth>=iid_depth&&pv_node&&!is_mv(hash_move)){
        pvs(sd,0,1,alpha,beta,depth-2,ply,0,0);
        hash_data=get_hash(sd);
        if(hash_data.raw) hash_move=hash_data.move;
      }
    }
    init_move_list(&move_list,n_search,pos->in_check);
    hash_bound=hash_upper;
    best_score=-mate_score+ply;
    move_t best_move=hash_move;
    int searched_cnt=0,quiet_moves_cnt=0,lmp_cnt=0;
    sd->killer_moves[ply+1][0]=sd->killer_moves[ply+1][1]=0;
    move_t move;
    int score=0,reduction=0,h_score=0,piece_pos=0,new_depth=0,beta_cut=0;
    while((move=next_move(&move_list,sd,hash_move,depth,ply))){
      if(eq(move,skip_move)) continue;
      lmp_cnt++;
      if(!root_node&&searched_cnt>=1){
        if(is_quiet(move)){
          if(depth<=lmp_depth&&lmp_cnt>lmp[improving][depth]){
            move_list.cnt=move_list.moves_cnt;
            continue;
          }
          if(depth<=cmhp_depth){
            piece_pos=pos->board[mv_from(move)]*board_size+mv_to(move);
            if((!cmh_ptr[0]||cmh_ptr[0][piece_pos]<0)&&
              (!cmh_ptr[1]||cmh_ptr[1][piece_pos]<0))
              continue;
          }
          if(see(pos,move,1)<see_quiets_margin(depth)) continue;
        }
        if(move_list.phase==bad_captures&&
          mv_score(move)<see_captures_margin(depth))
          continue;
      }
      if(!legal_move(pos,move)){
        lmp_cnt--;
        continue;
      }
      new_depth=depth-1;
      if(depth>=se_depth&&!skip_move&&eq(move,hash_move)&&
        !root_node&&!is_mate_score(hash_score)&&
        hash_data.bound==hash_lower&&hash_data.depth>=depth-3){
        beta_cut=hash_score-depth;
        score=pvs(sd,0,0,beta_cut-1,beta_cut,depth>>1,ply,0,move);
        if(score<beta_cut) new_depth++;
      } else if(is_quiet(move)){
        piece_pos=pos->board[mv_from(move)]*board_size+mv_to(move);
        if(cmh_ptr[0]&&cmh_ptr[1]&&
          cmh_ptr[0][piece_pos]>=max_history_score/2&&
          cmh_ptr[1][piece_pos]>=max_history_score/2)
          new_depth++;
      }
      make_move(sd,move);
      searched_cnt++;
      if(pv_node) pv[(ply+1)*ply_limit]=0;
      if(searched_cnt==1) score=-pvs(sd,0,pv_node,-beta,-alpha,new_depth,ply+1,1,0);
      else{
        reduction=0;
        if(depth>=lmr_depth&&is_quiet(move)){
          reduction=lmr[depth][searched_cnt];
          if(!improving) reduction++;
          if(reduction&&pv_node) reduction--;
          reduction-=2*get_h_score(sd,pos,cmh_ptr,move)/max_history_score;
          if(reduction>=new_depth) reduction=new_depth-1;
          else if(reduction<0) reduction=0;
        }
        score=-pvs(sd,0,0,-alpha-1,-alpha,new_depth-reduction,ply+1,1,0);
        if(reduction&&score>alpha) score=-pvs(sd,0,0,-alpha-1,-alpha,new_depth,ply+1,1,0);
        if(score>alpha&&score<beta) score=-pvs(sd,0,1,-beta,-alpha,new_depth,ply+1,1,0);
      }
      undo_move(sd);
      sd->nodes++;
      if(!search_state.go.infinite&&(sd->nodes&1023)==0){
        u64 elapsed=time_in_ms()-search_state.curr_time;
        if(u64 target_time=search_state.target_time[search_state.tsteps];target_time>0&&elapsed>=target_time){
          search_state.done=1;
          return 0;
        }
      }
      if(search_state.done) return 0;
      if(score>best_score){
        best_score=score;
        if(score>alpha){
          best_move=move;
          if(pv_node){
            if(root_node){
              if(eq(best_move,pv[0])){
                if(search_state.tsteps>0) search_state.tsteps--;
              } else search_state.tsteps=tmsteps-1;
              search_state.score=score;
              search_state.depth=depth;
            }
            update_pv(move,ply);
          }
          alpha=score;
          hash_bound=hash_exact;
          if(alpha>=beta){
            hash_bound=hash_lower;
            if(is_quiet(best_move)){
              h_score=d_score(depth+(score>beta+80));
              set_killer_move(sd,best_move,ply);
              set_counter_move(sd,best_move);
              add_to_history(sd,cmh_ptr,best_move,h_score);
              for(int i=0;i<quiet_moves_cnt;++i) add_to_history(sd,cmh_ptr,quiet_moves[i],-h_score);
            }
            break;
          }
        }
      }
      if(is_quiet(move)) quiet_moves[quiet_moves_cnt++]=move;
    }
    if(searched_cnt==0) return pos->in_check||skip_move?-mate_score+ply:0;
    if(use_hash) set_hash(sd,best_move,best_score,static_score,depth,ply,hash_bound);
    return best_score;
  }

  void main_search(search_data_t* sd){
    int score=0;
    int prev_score=0;
    for(int depth=1;depth<=search_state.max_depth;++depth){
      int delta=depth>=start_aspiration_depth
        ?static_cast<int>(init_aspiration_window)
        :static_cast<int>(mate_score);
      int alpha=std::max(score-delta,-mate_score);
      int beta=std::min(score+delta,static_cast<int>(mate_score));
      while(delta<=mate_score){
        score=pvs(sd,1,1,alpha,beta,depth,0,0,0);
        if(search_state.done) break;
        delta+=2+delta/2;
        if(score<=alpha){
          beta=(alpha+beta)/2;
          alpha=std::max(score-delta,-mate_score);
        } else if(score>=beta) beta=std::min(score+delta,static_cast<int>(mate_score));
        else break;
      }
      if(search_state.done) break;
      print_search_info(pv);
      u64 target_time=search_state.target_time[search_state.tsteps];
      if(prev_score>score) target_time*=static_cast<u64>(std::min(1.0 + static_cast<double>(prev_score - score) / 80.0, 2.0));
      prev_score=score;
      if(target_time>0&&depth>=min_depth_to_reach&&
        time_in_ms()-search_state.curr_time>=target_time)
        break;
    }
    print_search_info(pv);
    {
      search_state.search_finished=1;
      search_state.done=1;
    }
  }

  void print_best_move(){
    const move_t best_move=pv[0];
    SO<<"bestmove "<<to_str(best_move)<<SE;
  }

  void init_search_data(search_data_t* sd,const search_data_t* src_sd){
    sd->pos=sd->pos_list.data();
    position_cpy(sd->pos,src_sd->pos);
    sd->hash_key=src_sd->hash_key;
    sd->hash_keys_cnt=src_sd->hash_keys_cnt;
    sd->hash_keys=src_sd->hash_keys;
    sd->nodes=0;
  }
}

void reset_search_data(search_data_t* sd){
  const u64 hash_key=sd->hash_keys[0];
  memset(sd,0,sizeof(search_data_t));
  sd->pos=sd->pos_list.data();
  sd->hash_key=hash_key;
  for(auto& i:sd->counter_move_history) for(auto& j:i) for(short& k:j) k=-1;
}

void reset_all(){
  reset_search_data(search_config.sd);
  reset_hash(search_config.sd);
}

void search(void* unused){
  search_state.done=0;
  search_data_t* sd=search_config.sd;
  search_state.tsteps=0;
  search_state.curr_time=time_in_ms();
  set_hash_iteration();
  reevaluate_position(sd->pos);
  memset(pv,0,sizeof(pv));
  for(auto& cnt:search_depth_cnt) cnt.store(0,std::memory_order_relaxed);
  init_search_data(sd,sd);
  main_search(sd);
  print_best_move();
}

void set_killer_move(search_data_t* sd,const move_t move,const int ply){
  if(!eq(sd->killer_moves[ply][0],move)){
    const move_t tmp=sd->killer_moves[ply][0];
    sd->killer_moves[ply][0]=move;
    sd->killer_moves[ply][1]=tmp;
  }
}

void set_counter_move(search_data_t* sd,const move_t move){
  if(!is_mv(sd->pos->move)) return;
  const int to=mv_to(sd->pos->move);
  sd->counter_moves[sd->pos->board[to]][to]=move;
}

move_t get_counter_move(const search_data_t* sd){
  if(!is_mv(sd->pos->move)) return 0;
  const int to=mv_to(sd->pos->move);
  return sd->counter_moves[sd->pos->board[to]][to];
}

void add_to_history(search_data_t* sd,i16** cmh_ptr,const move_t move,const int score){
  const int to=mv_to(move);
  const int from=mv_from(move);
  const int piece_pos=sd->pos->board[from]*board_size+to;
  i16* item=&sd->history[sd->pos->side][from][to];
  *item+=score-*item*abs(score)/max_history_score;
  for(int i=0;i<max_cmh_ply;i++)
    if(cmh_ptr[i]){
      item=&cmh_ptr[i][piece_pos];
      *item+=score-*item*abs(score)/max_history_score;
    }
}

u64 perft(search_data_t* sd,const int depth,const int ply,const int additional_tests){
  int i,moves_cnt,check_ep;
  move_t moves[max_moves];
  const position_t* pos=sd->pos;
  u64 nodes=check_ep=0;
  if(pos->ep_sq!=n_sqs){
    const u64 p_b=pos->piece_occ[pawn]&pos->occ[pos->side];
    check_ep=!!(piece_area[o_piece(pos,pawn)][pos->ep_sq]&p_b);
  }
  if(pos->in_check){
    check_evasion_moves(pos,moves,&moves_cnt);
  } else if(depth>1||pos->pinned[pos->side]||check_ep){
    get_all_moves(pos,moves,&moves_cnt);
  } else{
    nodes+=non_king_moves(pos);
    king_moves(pos,moves,&moves_cnt);
  }
  if(depth==1){
    for(i=0;i<moves_cnt;i++) if(legal_move(pos,moves[i])) nodes++;
    return nodes;
  }
  for(i=0;i<moves_cnt;i++){
    if(!legal_move(pos,moves[i])) continue;
    make_move(sd,moves[i]);
    nodes+=perft(sd,depth-1,ply+1,additional_tests);
    undo_move(sd);
  }
  return nodes;
}

void divide(search_data_t* sd,const int depth){
  int moves_cnt=0;
  move_t moves[max_moves];
  const position_t* pos=sd->pos;
  get_all_moves(pos,moves,&moves_cnt);
  u64 total_nodes=0;
  const u64 start_time=time_in_ms();
  for(int i=0;i<moves_cnt;i++){
    if(!legal_move(pos,moves[i])) continue;
    make_move(sd,moves[i]);
    const u64 nodes=perft(sd,depth-1,1,0);
    undo_move(sd);
    total_nodes+=nodes;
    SO<<to_str(moves[i])<<": "<<nodes<<SE;
  }
  const u64 elapsed_time=time_in_ms()-start_time;
  const u64 nps=elapsed_time>0?total_nodes*1000/elapsed_time:0;
  SO<<"Nodes: "<<total_nodes<<SE;
  SO<<"Time: "<<elapsed_time<<"ms"<<SE;
  SO<<"NPS: "<<nps<<SE;
}
