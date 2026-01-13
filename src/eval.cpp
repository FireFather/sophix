#include "eval.h"
#include <algorithm>
#include "bitboard.h"
#include "main.h"
#include "position.h"

namespace{
  void rook_open_files(const int sq,eval_state& es,const int (*rook_file)[2]){
    if(const u64 b1=file[sq_file(sq)];!(b1&es.p_occ_f)){
      const int open_file=!(b1&es.p_occ_o);
      es.score_mg+=rook_file[es.phase_mg][open_file];
      es.score_eg+=rook_file[es.phase_eg][open_file];
    }
  }

  void score_threats(const int piece,const u64 b,eval_state& es,const int threat[2][4][8]){
    u64 b1=b&es.occ_o_nk;
    while(b1){
      const int sq=bsf(b1);
      const int piece_o=to_white(es.pos->board[sq]);
      es.score_mg+=threat[es.phase_mg][piece][piece_o];
      es.score_eg+=threat[es.phase_eg][piece][piece_o];
      b1&=b1-1;
    }
  }

  void threats_to_queen(const int piece,const int side,const u64 occ,const int sq,const u64 safe_area,
    const u64 (&piece_att)[n_sides][n_pieces],const u64 (&d_att_area)[n_sides],const int phase_mg,const int phase_eg,
    const int threats_on_queen[n_pieces][n_phases],int& score_mg,int& score_eg,u64 (*method)(u64,int)){
    u64 b=piece_att[side][piece]&method(occ,sq)&safe_area;
    if(piece!=knight) b&=d_att_area[side];
    if(b){
      const int pcnt=popcnt(b);
      score_mg+=pcnt*threats_on_queen[piece][phase_mg];
      score_eg+=pcnt*threats_on_queen[piece][phase_eg];
    }
  }

  template<typename MethodFunc,typename ThreatsFunc,typename RookBonusFunc> void score_piece(int piece,eval_state& es,MethodFunc method,ThreatsFunc threats_func,RookBonusFunc rook_func,const u64 att){
    es.piece_att[es.side][piece]=0;
    u64 b0=es.pos->piece_occ[piece]&es.occ_f;
    while(b0){
      int sq=bsf(b0);
      u64 b=method(es.occ_x,sq);
      es.piece_att[es.side][piece]|=b;
      es.d_att_area[es.side]|=es.att_area[es.side]&b;
      es.att_area[es.side]|=b;
      b&=es.moarea[es.side];
      threats_func(piece,b);
      const int pcnt=popcnt(b);
      es.score_mg+=mobility[es.phase_mg][piece][pcnt];
      es.score_eg+=mobility[es.phase_eg][piece][pcnt];
      b&=es.k_zone|att;
      if(b){
        es.k_cnt[es.side]++;
        es.k_score[es.side]+=popcnt(b&es.k_zone);
        es.checks[es.side]|=b&att;
        es.k_score[es.side]+=popcnt(b&att);
      }
      rook_func(sq);
      b0&=b0-1;
    }
  }

  int eval_pawn_shield(const int side,const int k_sq,const u64 p_occ_f,const u64 p_occ_o){
    int r;
    const int f=std::max(std::min(sq_file(k_sq),static_cast<int>(file_g)),static_cast<int>(file_b));
    const int m=side==white?7:0;
    int score=0;
    for(int fi=f-1;fi<=f+1;fi++){
      int r_min=n_ranks-1;
      u64 b=file[fi]&p_occ_f;
      for_each_set_bit(b,[&](const u64 bit){
        r=m^get_rank(bsf(bit));
        r_min=std::min(r_min,r);
      });
      score+=pawn_shield[sq_rf(r_min,fi)];
      b=file[fi]&p_occ_o;
      for_each_set_bit(b,[&](const u64 bit){
        const int sq=bsf(bit);
        r=m^get_rank(sq);
        const int unopposed=!(doubled_pawn_area[side^1][sq]&p_occ_f);
        score+=pawn_storm[unopposed][sq_rf(r,fi)];
      });
    }
    return score;
  }
}

int eval(position_t* pos){
  eval_state es={};
  es.pos=pos;
  es.phash_data=pawn_eval(pos);
  es.score_mg=pos->score_mg+es.phash_data.score_mg;
  es.score_eg=pos->score_eg+es.phash_data.score_eg;
  es.p_occ=pos->piece_occ[pawn];
  es.occ=occupancy(pos);
  es.pushed_passers=es.phash_data.pushed_passers;
  for(int i=0;i<n_sides;++i){
    es.k_score[i]=0;
    es.k_cnt[i]=0;
    es.pushed[i]=0;
    es.moarea[i]=0;
    es.att_area[i]=0;
    es.d_att_area[i]=0;
    es.checks[i]=0;
    for(int j=0;j<n_pieces;++j) es.piece_att[i][j]=0;
  }
  for(es.side=white;es.side<n_sides;es.side++){
    es.p_occ_f=es.p_occ&pos->occ[es.side];
    es.pushed[es.side]=pushed_pawns(es.p_occ_f,~es.occ,es.side);
    es.piece_att[es.side][pawn]=pawn_atts(es.p_occ_f,es.side);
  }
  for(es.side=white;es.side<n_sides;es.side++){
    es.phase_mg=phase_mg;
    es.phase_eg=phase_eg;
    es.k_sq_f=pos->k_sq[es.side];
    es.k_sq_o=pos->k_sq[es.side^1];
    es.k_zone=king_zone[es.k_sq_o];
    es.occ_f=pos->occ[es.side];
    es.occ_o=pos->occ[es.side^1];
    es.p_occ_f=es.p_occ&es.occ_f;
    es.p_occ_o=es.p_occ&es.occ_o;
    es.occ_o_nk=es.occ_o&~bb(es.k_sq_o);
    es.occ_x=es.occ^pos->piece_occ[queen];
    es.n_att=knight_att(es.occ,es.k_sq_o);
    es.att=bishop_att(es.occ_x,es.k_sq_o);
    es.r_att=rook_att(es.occ_x,es.k_sq_o);
    es.moarea[es.side]=~(es.p_occ_f|es.piece_att[es.side^1][pawn]|bb(es.k_sq_f));
    es.piece_att[es.side][king]=piece_area[king][pos->k_sq[es.side]];
    es.att_area[es.side]=es.piece_att[es.side][pawn]|es.piece_att[es.side][king];
    es.d_att_area[es.side]=es.piece_att[es.side][pawn]&es.piece_att[es.side][king];
    es.checks[es.side]=0;
    es.k_score[es.side]=es.k_cnt[es.side]=0;
    score_piece(knight,es,
      knight_att,
      [&](const int piece,const u64 b){
        score_threats(piece,b,es,threats);
      },
      [](int){},
      es.n_att
      );
    score_piece(bishop,es,
      bishop_att,
      [&](const int piece,const u64 b){
        score_threats(piece,b,es,threats);
      },
      [](int){},
      es.att
      );
    es.att|=es.r_att;
    score_piece(queen,es,
      queen_att,
      [](int,u64){},
      [](int){},
      es.att
      );
    es.occ_x^=pos->piece_occ[rook]&es.occ_f&~(es.side==white?rank1:rank8);
    es.piece_att[es.side][rook]=0;
    es.b0=pos->piece_occ[rook]&es.occ_f;
    for_each_set_bit(es.b0,[&](const u64 bit0){
      es.sq=bsf(bit0);
      es.b=rook_att(es.occ_x,es.sq);
      es.piece_att[es.side][rook]|=es.b;
      es.d_att_area[es.side]|=es.att_area[es.side]&es.b;
      es.att_area[es.side]|=es.b;
      es.b&=es.moarea[es.side];
      es.b1=es.b&es.occ_o_nk;
      for_each_set_bit(es.b1,[&](const u64 bit1){
        es.piece_o=to_white(pos->board[bsf(bit1)]);
        es.score_mg+=threats[phase_mg][rook][es.piece_o];
        es.score_eg+=threats[phase_eg][rook][es.piece_o];
      });
      es.pcnt=popcnt(es.b);
      es.score_mg+=mobility[phase_mg][rook][es.pcnt];
      es.score_eg+=mobility[phase_eg][rook][es.pcnt];
      es.b&=es.k_zone|(es.r_att);
      if(es.b){
        es.k_cnt[es.side]++;
        es.k_score[es.side]+=popcnt(es.b&es.k_zone);
        es.checks[es.side]|=es.b&(es.r_att);
        es.k_score[es.side]+=popcnt(es.b);
      }
      rook_open_files(es.sq,es,rook_file);
    });
    es.score_eg+=popcnt(es.att_area[es.side]&es.pushed_passers)*pushed_passers;
    if(es.piece_att[es.side][king]&es.moarea[es.side]&es.occ_o){
      es.score_mg+=king_threat[phase_mg];
      es.score_eg+=king_threat[phase_eg];
    }
    es.b=(es.side==white?es.p_occ<<8:es.p_occ>>8)&es.occ_f&(pos->piece_occ[knight]|pos->piece_occ[bishop]);
    es.score_mg+=popcnt(es.b)*behind_pawn;
    if(popcnt(pos->piece_occ[bishop]&es.occ_f)>=2){
      es.score_mg+=bishop_pair[phase_mg];
      es.score_eg+=bishop_pair[phase_eg];
    }
    es.score_mg=-es.score_mg;
    es.score_eg=-es.score_eg;
  }
  for(es.side=white;es.side<n_sides;es.side++){
    es.occ_f=pos->occ[es.side];
    es.occ_o=pos->occ[es.side^1];
    es.p_occ_f=es.p_occ&es.occ_f;
    es.p_occ_o=es.p_occ&es.occ_o;
    es.occ_o_np=es.occ_o&~es.p_occ_o;
    es.safe_area=es.att_area[es.side]|~es.att_area[es.side^1];
    es.pcnt=popcnt(es.pushed[es.side]&es.safe_area);
    es.score_mg+=es.pcnt*pawn_mobility[phase_mg];
    es.score_eg+=es.pcnt*pawn_mobility[phase_eg];
    es.p_safe_att=pawn_atts(es.p_occ_f&es.safe_area,es.side);
    es.k_score[es.side]+=popcnt(es.p_safe_att&king_zone[pos->k_sq[es.side^1]])*pawn_att;
    es.pcnt=popcnt(es.p_safe_att&es.occ_o_np);
    es.score_mg+=es.pcnt*prot_pawn[phase_mg];
    es.score_eg+=es.pcnt*prot_pawn[phase_eg];
    es.pcnt=popcnt(pawn_atts(es.pushed[es.side]&es.safe_area,es.side)&es.occ_o_np);
    es.score_mg+=es.pcnt*prot_pawn_push[phase_mg];
    es.score_eg+=es.pcnt*prot_pawn_push[phase_eg];
    es.b=es.checks[es.side]&~es.occ_f;
    es.b&=~es.att_area[es.side^1]|
    (es.d_att_area[es.side]&~es.d_att_area[es.side^1]&
      (es.piece_att[es.side^1][king]|es.piece_att[es.side^1][queen]));
    if(es.b){
      es.k_cnt[es.side]++;
      es.k_score[es.side]+=popcnt(es.b)*safe_check;
    }
    es.b=es.piece_att[es.side^1][king]&es.att_area[es.side]&~es.d_att_area[es.side^1];
    es.k_score[es.side]+=popcnt(es.b)*k_sq_att;
    es.score_mg+=sqr(es.k_score[es.side])*k_cnt_mul[std::min(es.k_cnt[es.side],k_cnt_limit-1)]/8;
    es.b=pos->piece_occ[queen]&es.occ_o;
    if(es.b){
      es.sq=bsf(es.b);
      es.safe_area=es.moarea[es.side]&(~es.att_area[es.side^1]|(es.d_att_area[es.side]&~es.d_att_area[es.side^1]));
      threats_to_queen(knight,es.side,es.occ,es.sq,es.safe_area,es.piece_att,es.d_att_area,phase_mg,phase_eg,queen_threats,es.score_mg,es.score_eg,knight_att);
      threats_to_queen(bishop,es.side,es.occ,es.sq,es.safe_area,es.piece_att,es.d_att_area,phase_mg,phase_eg,queen_threats,es.score_mg,es.score_eg,bishop_att);
      threats_to_queen(rook,es.side,es.occ,es.sq,es.safe_area,es.piece_att,es.d_att_area,phase_mg,phase_eg,queen_threats,es.score_mg,es.score_eg,rook_att);
    }
    es.score_mg=-es.score_mg;
    es.score_eg=-es.score_eg;
  }
  if(pos->side==black){
    es.score_mg=-es.score_mg;
    es.score_eg=-es.score_eg;
  }
  es.initiative=
    initiative[0]*popcnt(es.p_occ)+
    initiative[1]*(es.p_occ&q_side&&es.p_occ&k_side)+
    initiative[2]*(popcnt(es.occ&~es.p_occ)==2)-
    initiative[3];
  es.score_eg+=sign(es.score_eg)*std::max(es.initiative,-abs(es.score_eg));
  int score;
  if(pos->phase>=total_phase) score=es.score_eg;
  else
    score=(es.score_mg*(total_phase-pos->phase)+
      es.score_eg*pos->phase)>>phase_shift;
  return score+tempo;
}

phash_data_t pawn_eval(const position_t* pos){
  phash_data_t phash_data;
  if(get_phash(pos,&phash_data)) return phash_data;
  u64 pushed_passers=0;
  const u64 p_occ=pos->piece_occ[pawn];
  int score_mg=0;
  int score_eg=0;
  for(int side=white;side<n_sides;side++){
    const int k_sq_f=pos->k_sq[side];
    const int k_sq_o=pos->k_sq[side^1];
    const u64 p_occ_f=p_occ&pos->occ[side];
    const u64 p_occ_o=p_occ&pos->occ[side^1];
    const int m=side==white?7:0;
    const u64 b=p_occ_f;
    int d_max=0;
    for_each_set_bit(b,[&](const u64 bit){
      const int sq=bsf(bit);
      const int f=sq_file(sq);
      const int r=m^get_rank(sq);
      const int rsq=side==white?sq:sq^56;
      const int ssq=side==white?sq-8:sq+8;
      const int d=distance[ssq][k_sq_o]*r-distance[ssq][k_sq_f]*(r-1);
      d_max=std::max(d,d_max);
      if(!(passer_area[side][sq]&p_occ_o)){
        pushed_passers|=bb(ssq);
        score_mg+=passer[phase_mg][rsq];
        score_eg+=passer[phase_eg][rsq]
          +distance[ssq][k_sq_o]*proximity[0][r]
          -distance[ssq][k_sq_f]*proximity[1][r];
      }
      if(connected_pawn_area[side][sq]&p_occ_f){
        const int unopposed=!(doubled_pawn_area[side][sq]&p_occ_o);
        score_mg+=connected[unopposed][phase_mg][rsq];
        score_eg+=connected[unopposed][phase_eg][rsq];
      } else{
        const u64 p_occ_x=p_occ_f^bb(sq);
        if(doubled_pawn_area[side][sq]&p_occ_x){
          score_mg-=doubled[phase_mg];
          score_eg-=doubled[phase_eg];
        }
        if(!(passer_area[side^1][ssq]&~file[f]&p_occ_x)&&
          (piece_area[pawn|(side<<side_shift)][ssq]|bb(ssq))&p_occ_o){
          score_mg-=backward[phase_mg];
          score_eg-=backward[phase_eg];
        }
        if(!(isolated_pawn_area[f]&p_occ_x)){
          score_mg-=isolated[phase_mg];
          score_eg-=isolated[phase_eg];
        }
      }
    });
    score_mg+=eval_pawn_shield(side,k_sq_f,p_occ_f,p_occ_o);
    score_eg+=d_max<<distance_bonus_shift;
    score_mg=-score_mg;
    score_eg=-score_eg;
  }
  return set_phash(pos,pushed_passers,score_mg,score_eg);
}

void eval_material_moves(const position_t* pos,move_t* moves,const int moves_cnt){
  for(int i=0;i<moves_cnt;i++){
    const move_t move=moves[i];
    const int piece=pos->board[mv_to(move)];
    if(!promoted_to(move)) moves[i]=with_score(move,piece);
  }
}

int eval_quiet_moves(search_data_t* sd,move_t* moves,const int moves_cnt,const int ply){
  i16* cmh_ptr[max_cmh_ply];
  const move_t killer_0=base(sd->killer_moves[ply][0]);
  const move_t killer_1=base(sd->killer_moves[ply][1]);
  const move_t counter=base(get_counter_move(sd));
  set_counter_move_history_pointer(cmh_ptr,sd,ply);
  int cnt=0;
  for(int i=0;i<moves_cnt;i++){
    if(const move_t move=base(moves[i]);move!=killer_0&&move!=killer_1&&move!=counter)
      moves[cnt++]=set_quiet(
        with_score(move,get_h_score(sd,sd->pos,cmh_ptr,move))
        );
  }
  return cnt;
}

void eval_all_moves(search_data_t* sd,move_t* moves,const int moves_cnt,const int ply){
  int score;
  i16* cmh_ptr[max_cmh_ply];
  const position_t* pos=sd->pos;
  set_counter_move_history_pointer(cmh_ptr,sd,ply);
  for(int i=0;i<moves_cnt;i++){
    if(const move_t move=moves[i];move_is_quiet(pos,move)) moves[i]=set_quiet(with_score(move,get_h_score(sd,pos,cmh_ptr,move)));
    else{
      if(promoted_to(moves[i])) score=1;
      else{
        const int piece=pos->board[mv_to(move)];
        score=p_limit+((piece==empty?0:piece)<<5)-
          pos->board[mv_from(move)];
      }
      moves[i]=with_score(move,score+3*max_history_score);
    }
  }
}

int pst_mg[p_limit][board_size],pst_eg[p_limit][board_size];
int distance[board_size][board_size];
const int piece_value[n_pieces]={100,310,330,500,1000,20000};
const int piece_phase[n_pieces]={0,6,6,13,28,0};

const int pst_mg_base[n_pieces][board_size]={
{
0,0,0,0,0,0,0,0,
14,17,-3,23,35,33,-6,-27,
-13,-3,21,20,68,41,9,-7,
-15,-2,-12,-1,-2,-4,-3,-15,
-21,-26,-10,-18,-9,-9,-21,-42,
-30,-24,-31,-36,-24,-27,-10,-39,
-30,-17,-23,-25,-25,-5,13,-26,
0,0,0,0,0,0,0,0
},
{
-135,-87,-46,-25,19,-118,-113,-84,
7,18,45,66,39,69,34,31,
16,37,53,60,82,90,53,32,
52,57,63,74,66,73,61,70,
53,47,64,64,75,73,76,64,
29,42,42,49,54,56,59,40,
19,26,35,43,51,40,45,35,
-5,33,19,38,45,30,33,9
},
{
40,3,-64,-63,-57,-84,-18,6,
-1,9,13,1,21,9,-9,-24,
16,18,27,45,57,58,28,34,
21,49,35,51,46,48,57,21,
30,34,42,55,61,47,47,58,
33,48,39,39,41,48,48,39,
44,39,44,28,34,47,57,41,
32,42,30,18,29,20,16,41
},
{
15,-1,-17,13,1,-30,-2,18,
8,-11,12,32,1,11,-27,9,
-2,19,6,19,40,36,22,-6,
-7,2,15,15,29,30,6,5,
-10,-12,-4,9,12,10,22,-13,
-14,-1,-4,4,5,6,12,-12,
-9,-7,1,8,13,4,3,-46,
4,8,12,20,22,20,-5,2
},
{
-31,-3,23,16,13,29,30,15,
-14,-5,7,-7,-55,-13,-23,8,
8,23,25,31,23,22,6,-1,
2,24,22,12,33,25,31,14,
28,20,22,13,35,26,42,29,
14,30,18,22,23,26,43,23,
14,23,32,29,29,42,42,15,
22,14,18,25,16,8,-4,9
},
{
5,20,75,-23,-31,-53,78,94,
13,82,42,50,29,59,-24,-51,
-22,55,142,60,80,162,70,-39,
-51,32,68,43,51,67,26,-163,
-56,44,32,16,12,1,3,-115,
-68,-1,-4,-3,-11,-9,7,-47,
-13,-6,-5,-41,-33,-13,17,9,
-7,10,3,-50,2,-34,26,7
}
};
const int pst_eg_base[n_pieces][board_size]={
{
1,-1,1,-1,-1,-1,1,1,
8,15,28,30,38,26,31,14,
7,7,2,4,18,2,12,7,
3,-6,-7,-17,-12,-3,-1,-4,
-7,-4,-19,-24,-20,-14,-7,-15,
-12,-10,-17,-11,-13,-9,-17,-19,
-12,-7,-9,-12,-3,-5,-11,-21,
1,1,-1,1,0,1,1,-1
},
{
-47,-16,-8,-18,-24,-27,-22,-70,
-16,-4,-17,-9,1,-7,-7,-37,
-6,-1,16,14,9,5,0,-22,
-5,0,17,20,15,11,-2,-13,
-6,6,18,19,17,16,1,0,
-13,-6,6,15,10,-2,-13,-11,
-27,-10,-15,0,-5,-17,-6,-13,
-23,-23,-15,-5,-14,-17,-20,-41
},
{
-8,-6,-9,-3,-9,5,-6,-17,
-5,2,-1,-2,-3,-6,-7,-14,
-6,2,5,2,5,6,-4,-10,
-6,-1,1,14,12,2,-6,-14,
-5,-3,9,12,9,6,-7,-19,
-11,-1,1,3,3,5,-6,-7,
-11,-14,-14,-4,-7,-11,-15,-29,
-13,-19,-11,-11,-11,-5,-20,-22
},
{
31,38,41,38,37,39,39,39,
29,33,34,32,29,25,33,29,
30,27,30,28,21,27,18,24,
30,32,30,29,24,22,22,21,
27,31,30,25,25,22,19,19,
26,26,29,19,20,15,11,17,
14,20,24,19,13,8,9,15,
22,18,22,14,16,10,19,15
},
{
34,40,33,35,21,40,21,24,
52,46,56,61,69,62,35,31,
36,46,36,54,50,22,15,1,
60,52,59,65,54,36,40,30,
21,48,51,63,43,57,48,42,
24,23,45,36,44,49,37,15,
21,13,2,19,24,-7,-12,-2,
5,2,0,-8,13,-4,6,14
},
{
-41,-24,-3,2,0,-20,-37,-32,
-11,18,12,10,19,21,32,-11,
1,15,27,24,26,33,24,6,
-2,20,22,20,18,21,22,6,
-3,14,19,17,16,16,10,-2,
-6,10,12,12,12,15,6,-7,
-21,0,9,6,7,10,3,-17,
-47,-21,-7,-3,-18,-2,-15,-42
}
};
const int pawn_shield[board_size]={
-1,0,-1,1,0,-1,1,-1,
1,21,22,10,8,21,13,4,
10,22,-5,3,6,5,8,14,
2,-3,-5,3,2,-4,-10,8,
4,-15,7,-4,-9,1,-8,-3,
35,85,54,-19,-34,-8,22,-5,
43,47,94,37,74,37,6,-30,
-31,-33,-15,-6,-6,-11,-23,-29
};
const int pawn_storm[2][board_size]={
{
0,1,1,0,0,1,0,0,
-1,0,0,1,-1,0,1,-1,
-38,-59,-16,-29,-31,-7,-31,-7,
28,-3,-11,-15,1,0,7,26,
21,3,-2,1,10,-4,-4,9,
15,34,1,3,16,-9,8,9,
10,28,2,3,2,6,0,6,
-1,-1,0,0,1,0,0,-1
},
{
1,-1,0,0,-1,-1,-1,0,
56,23,-25,17,-54,-45,20,82,
-20,-125,-83,-10,-58,-93,-78,-28,
-20,0,-35,-22,-17,-17,-6,-17,
11,17,-16,-11,-9,-6,5,-4,
10,44,7,7,8,2,28,20,
29,30,3,5,-4,11,11,13,
1,1,-1,-1,-1,1,0,1
}
};
const int connected[2][n_phases][board_size]={
{
{
1,-1,-1,1,1,1,1,0,
1,-1,0,1,0,1,-1,0,
5,15,-16,40,14,31,-22,30,
2,0,4,11,26,32,17,15,
-3,14,8,9,12,13,22,17,
5,10,17,17,16,7,25,16,
9,0,2,-4,1,-5,8,2,
0,0,1,0,-1,-1,0,0
},
{
-1,0,-1,-1,0,-1,1,-1,
1,1,0,1,-1,0,-1,-1,
28,27,45,33,23,37,37,9,
8,12,1,4,8,0,5,-7,
2,2,5,13,9,4,-2,1,
-1,5,10,8,13,4,0,-3,
-4,0,0,2,4,-6,1,-21,
0,0,1,0,0,-1,1,1
}
},
{
{
0,1,0,0,-1,-1,1,0,
147,125,157,171,190,151,62,36,
34,39,76,64,51,53,95,45,
43,41,46,41,54,31,36,40,
22,30,20,32,32,24,31,29,
22,31,24,28,30,19,34,25,
17,6,5,20,18,-2,8,6,
1,0,-1,0,0,0,1,0
},
{
0,-1,1,0,-1,1,0,-1,
57,91,48,91,78,96,65,65,
76,65,67,56,55,57,35,56,
31,37,37,35,34,30,27,26,
12,9,17,25,19,13,9,11,
-1,5,8,10,9,3,2,-1,
-17,-6,-5,-4,-4,-8,1,-19,
-1,0,0,0,1,-1,-1,0
}
}
};
const int passer[n_phases][board_size]={
{
0,0,1,-1,0,-1,0,0,
110,94,92,94,99,75,89,121,
65,36,22,38,-4,29,-9,19,
33,11,14,11,9,6,24,7,
23,3,-16,-3,-13,-6,-20,8,
-1,-3,-13,-10,-19,-14,-37,11,
-5,-7,-7,0,9,-40,-31,-1,
1,1,1,-1,1,0,1,-1
},
{
-1,-1,-1,0,1,-1,-1,0,
102,110,110,108,108,114,114,114,
60,69,73,71,63,81,85,78,
39,41,35,37,38,39,44,41,
26,24,26,22,22,23,32,27,
8,3,6,1,4,1,12,2,
2,8,-3,-2,-8,-4,6,2,
0,0,0,1,-1,-1,-1,-1
}
};

const int mobility[n_phases][n_pieces-1][32]={
{
{0,0},
{-2,19,30,41,48,56,62,65,71},
{0,14,27,30,41,46,47,47,47,48,57,64,70,100},
{-68,5,19,23,27,25,27,33,33,37,35,36,39,46,44},
{
-170,11,10,12,14,13,17,17,20,20,22,20,24,22,
19,20,20,13,12,15,12,19,36,-7,28,23,0,12
},
},
{
{0,0},
{-68,-16,8,16,19,26,25,21,6},
{-38,-11,-3,10,15,23,27,28,30,27,27,15,23,5},
{-27,7,36,44,48,54,59,59,60,65,68,67,69,68,64},
{
-12,-25,-31,14,15,42,51,64,58,76,88,96,96,102,
110,109,103,110,105,97,87,74,55,62,60,57,68,74
},
}
};
const int threats[n_phases][n_pieces-2][8]={
{
{0,0},
{-3,-5,26,57,27},
{4,23,6,47,37},
{2,21,22,6,29},
},
{
{0,0},
{17,3,33,17,2},
{13,33,21,21,34},
{16,25,34,11,19},
}
};
const int proximity[2][n_ranks]={
{0,1,3,7,18,39,45,-1},
{-1,3,2,7,10,17,21,0},
};
const int queen_threats[n_pieces][n_phases]={
{0,0},{17,-8},{21,18},{21,5},{0,0},{0,0}
};

const int doubled[n_phases]={9,24};
const int backward[n_phases]={-1,2};
const int isolated[n_phases]={5,3};
const int king_threat[n_phases]={15,27};
const int prot_pawn[n_phases]={54,44};
const int prot_pawn_push[n_phases]={13,13};
const int rook_file[n_phases][2]={{12,26},{1,4}};
const int bishop_pair[n_phases]={41,41};
const int pawn_mobility[n_phases]={2,8};
const int initiative[4]={5,66,72,89};

void init_distance(){
  for(int i=0;i<board_size;i++)
    for(int j=0;j<board_size;j++)
      distance[i][j]=std::min(std::max(abs(get_rank(i)-get_rank(j)),
        abs(sq_file(i)-sq_file(j))),5);
}

void init_pst(){
  memset(pst_mg,0,sizeof(pst_mg));
  memset(pst_eg,0,sizeof(pst_eg));
  for(int piece=0;piece<n_pieces;piece++)
    for(int sq=0;sq<board_size;sq++){
      int v=piece_value[piece]+pst_mg_base[piece][sq];
      pst_mg[piece][sq]=v;
      pst_mg[piece|change_side][sq^56]=v;
      v=piece_value[piece]+pst_eg_base[piece][sq];
      pst_eg[piece][sq]=v;
      pst_eg[piece|change_side][sq^56]=v;
    }
}
