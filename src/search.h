#pragma once
#include <array>
#include <atomic>
#include "position.h"

extern int lmr[max_depth][max_moves];
extern std::atomic<int> search_depth_cnt[max_depth];
extern move_t pv[ply_limit * ply_limit];

enum:u8{
  q_search=0,n_search=1,razor_depth=3,futility_depth=6,probcut_depth=5,iid_depth=5,lmp_depth=10,cmhp_depth=3,
  lmr_depth=3,se_depth=8,min_depth_to_reach=4,start_aspiration_depth=4,razor_margin=200,probcut_margin=80,init_aspiration_window=6,incheck=7,
  material_move=8,killer_move_0=9,killer_move_1=10,counter_move=11,quiet_move=12,bad_captures=13,end=14
};

enum:i8{
  min_hash_depth=-2
};

enum:u16{
  max_game_ply=1024,tmsteps=10,max_cmh_ply=2,max_history_score=1<<13
};

static const int lmp[][lmp_depth+1]={
{0,2,3,5,9,13,18,25,34,45,55},
{0,5,6,9,14,21,30,41,55,69,84}
};

struct search_data_t{
  int hash_keys_cnt;
  u64 nodes,hash_key;

  position_t* pos;
  std::array<position_t,ply_limit> pos_list;

  std::array<std::array<move_t,max_killer_moves>,ply_limit> killer_moves;
  std::array<std::array<move_t,board_size>,p_limit> counter_moves;

  std::array<std::array<std::array<i16,board_size>,board_size>,n_sides> history;
  std::array<std::array<std::array<i16,p_limit*board_size>,board_size>,p_limit> counter_move_history;

  std::array<u64,max_game_ply> hash_keys;
};

struct search_state_t{
  int max_depth,done,search_finished,score,depth,tsteps;
  u64 curr_time,max_time,target_time[tmsteps];

  struct{
    int infinite,time,inc,movestogo,depth,movetime;
  } go;
};

struct search_t{
  search_data_t* sd;
};

struct move_list_t{
  int search_mode,phase,cnt,moves_cnt,bad_captures_cnt,searched_hash_move;
  std::array<move_t,max_captures> bad_captures;
  std::array<move_t,max_moves> moves;
};

inline void set_counter_move_history_pointer(i16** cmh_ptr,search_data_t* sd,const int ply){
  for(int i=0;i<max_cmh_ply;i++){
    if(const position_t* pos=sd->pos-i;ply>i&&pos->move){
      const int to=mv_to(pos->move);
      cmh_ptr[i]=sd->counter_move_history[pos->board[to]][to].data();
    } else{
      cmh_ptr[i]=nullptr;
    }
  }
}

inline int get_h_score(const search_data_t* sd,const position_t* pos,
  i16** cmh_ptr,const move_t move){
  const int to=mv_to(move);
  const int from=mv_from(move);
  int score=sd->history[pos->side][from][to];
  if(cmh_ptr){
    const int piece_pos=pos->board[from]*board_size+to;
    for(int i=0;i<max_cmh_ply;i++) if(cmh_ptr[i]) score+=cmh_ptr[i][piece_pos];
  }
  return score;
}

inline void undo_move(search_data_t* sd){
  sd->pos--;
  sd->hash_keys_cnt--;
  sd->hash_key=sd->hash_keys[sd->hash_keys_cnt];
}

move_t get_counter_move(const search_data_t*);
move_t next_move(move_list_t*,search_data_t*,move_t,int,int);
u64 perft(search_data_t*,int,int,int);
void divide(search_data_t* sd,int);
void add_to_history(search_data_t*,i16**,move_t,int);
void reset_all();
void init_lmr();
void init_move_list(move_list_t*,int,int);
void init_castle();
void make_move_rev(search_data_t*,move_t);
void make_move(search_data_t*,move_t);
void make_null_move(search_data_t*);
void reset_search_data(search_data_t*);
void set_counter_move(search_data_t*,move_t);
void set_killer_move(search_data_t*,move_t,int);
void search(void* unused);

extern search_t search_config;
extern search_state_t search_state;
