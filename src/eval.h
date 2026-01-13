#pragma once
#include "main.h"
#include "hash.h"
#include "position.h"

enum:u8{
  safe_check=3,castling=20,pushed_passers=9,behind_pawn=9,pawn_att=2,distance_bonus_shift=2,k_sq_att=2,k_cnt_limit=8,
  tempo=10
};

inline const int k_cnt_mul[k_cnt_limit]={0,3,7,12,16,18,19,20};

int eval_quiet_moves(search_data_t*,move_t*,int,int);
int eval(position_t*);
phash_data_t pawn_eval(const position_t*);
void eval_all_moves(search_data_t*,move_t*,int,int);
void eval_material_moves(const position_t*,move_t*,int);
void init_distance();
void init_pst();

extern const int backward[n_phases];
extern const int bishop_pair[n_phases];
extern const int connected[2][n_phases][board_size];
extern const int proximity[2][n_ranks];
extern const int doubled[n_phases];
extern const int initiative[4];
extern const int isolated[n_phases];
extern const int mobility[n_phases][n_pieces-1][32];
extern const int passer[n_phases][board_size];
extern const int pawn_mobility[n_phases];
extern const int pawn_shield[board_size];
extern const int pawn_storm[2][board_size];
extern const int piece_phase[n_pieces];
extern const int piece_value[n_pieces];
extern const int rook_file[n_phases][2];
extern const int king_threat[n_phases];
extern const int prot_pawn_push[n_phases];
extern const int prot_pawn[n_phases];
extern const int queen_threats[n_pieces][n_phases];
extern const int threats[n_phases][n_pieces-2][8];
extern int distance[board_size][board_size];
extern int pst_mg[p_limit][board_size],pst_eg[p_limit][board_size];

struct eval_state{
  position_t* pos;
  int side,phase_mg,phase_eg;
  int score_mg,score_eg;
  int pcnt,sq,k_sq_f,k_sq_o,piece_o,initiative;
  int k_score[n_sides],k_cnt[n_sides];
  u64 b,b0,b1,k_zone,occ,occ_f,occ_o,occ_o_np,occ_o_nk,occ_x,p_occ,p_occ_f,p_occ_o;
  u64 n_att,att,r_att;
  u64 pushed_passers,safe_area,p_safe_att;
  u64 pushed[n_sides],moarea[n_sides],att_area[n_sides],d_att_area[n_sides],checks[n_sides];
  u64 piece_att[n_sides][n_pieces];
  phash_data_t phash_data;
};
