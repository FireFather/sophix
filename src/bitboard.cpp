#include "bitboard.h"
#include "eval.h"
#include "search.h"

struct att_vec_t{
  int r,f;
};

const att_vec_t att_vec[n_pieces][9]={
{{.r=-1,.f=1},{.r=-1,.f=-1},{.r=0,.f=0}},
{{.r=1,.f=2},{.r=1,.f=-2},{.r=-1,.f=2},{.r=-1,.f=-2},{.r=2,.f=1},{.r=-2,.f=1},{.r=2,.f=-1},{.r=-2,.f=-1},{.r=0,.f=0}},
{{.r=1,.f=1},{.r=1,.f=-1},{.r=-1,.f=1},{.r=-1,.f=-1},{.r=0,.f=0}},
{{.r=1,.f=0},{.r=-1,.f=0},{.r=0,.f=1},{.r=0,.f=-1},{.r=0,.f=0}},
{{.r=1,.f=1},{.r=1,.f=-1},{.r=-1,.f=1},{.r=-1,.f=-1},{.r=1,.f=0},{.r=-1,.f=0},{.r=0,.f=1},{.r=0,.f=-1},{.r=0,.f=0}},
{{.r=1,.f=1},{.r=1,.f=-1},{.r=-1,.f=1},{.r=-1,.f=-1},{.r=1,.f=0},{.r=-1,.f=0},{.r=0,.f=1},{.r=0,.f=-1},{.r=0,.f=0}},
};

std::array<att_lookup_t,board_size> bishop_att_lookup;
std::array<att_lookup_t,board_size> rook_att_lookup;
std::array<std::array<u64,board_size>,p_limit> piece_area;
std::array<std::array<u64,board_size>,n_sides> passer_area;
std::array<std::array<u64,board_size>,n_sides> connected_pawn_area;
std::array<std::array<u64,board_size>,n_sides> doubled_pawn_area;
std::array<u64,n_files> isolated_pawn_area;
std::array<u64,n_files> file;
std::array<u64,board_size> king_zone;
std::array<std::array<u64,board_size>,board_size> bline;

void init(){
  init_bbs();
  init_castle();
  init_distance();
  init_pst();
  init_lmr();
}

namespace{
  u64 bishop_atts_table[5248];
  u64 rook_atts_table[102400];

  u64 piece_att(const u64 occ,const int piece,const int sq){
    u64 b=0;
    const int wp=to_white(piece);
    const att_vec_t* v=att_vec[wp];
    const u32 r_sq=get_rank(sq);
    const u32 f_sq=sq_file(sq);
    for(;v->r||v->f;v++){
      int vr=v->r;
      const int vf=v->f;
      if(piece==(pawn|change_side)) vr=-vr;
      u32 r=r_sq+vr;
      u32 f=f_sq+vf;
      while(r<n_ranks&&f<n_files){
        const u64 square=bb(static_cast<u8>(sq_rf(static_cast<int>(r), static_cast<int>(f))));
        b|=square;
        if(wp==pawn||wp==knight||wp==king||occ&square) break;
        r+=vr;
        f+=vf;
      }
    }
    return b;
  }

  void init_piece_area(){
    for(int side=0;side<n_sides;side++)
      for(int p=pawn;p<=king;p++)
        for(int sq=0;sq<board_size;sq++){
          const int piece=p|(side?change_side:0);
          piece_area[piece][sq]=piece_att(0,piece,sq);
        }
  }

  void init_pawn_boards(){
    int r,f,fi;
    u64 b,bd,bc,bi,bf;
    for(int sq=0;sq<board_size;sq++)
      for(int side=0;side<n_sides;side++){
        u64 bp=bd=bc=bi=0;
        const int inc=side==white?-1:1;
        r=get_rank(sq);
        f=sq_file(sq);
        for(fi=f-1;fi<=f+1;fi++)
          if(fi>=0&&fi<n_files){
            for(int ri=r+inc;ri>=0&&ri<n_ranks;ri+=inc){
              b=bb(sq_rf(ri,fi));
              bp|=b;
              if(fi==f) bd|=b;
            }
            if(fi!=f) bc|=bb(sq_rf(r,fi))|bb(sq_rf(r-inc,fi));
          }
        passer_area[side][sq]=bp;
        doubled_pawn_area[side][sq]=bd;
        connected_pawn_area[side][sq]=bc;
      }
    for(f=0;f<n_files;f++){
      bi=bf=0;
      for(fi=f-1;fi<=f+1;fi++)
        if(fi>=0&&fi<n_files)
          for(r=0;r<n_ranks;r++){
            b=bb(sq_rf(r,fi));
            bi|=b;
            if(fi==f) bf|=b;
          }
      isolated_pawn_area[f]=bi;
      file[f]=bf;
    }
  }

  void init_king_zone(){
    for(int sq=0;sq<board_size;sq++){
      u64 b=piece_area[king][sq]|bb(sq);
      if(get_rank(sq)+(1&6)==0) b|=piece_area[king][sq^8];
      king_zone[sq]=b;
    }
    king_zone[a1]=king_zone[b1];
    king_zone[a8]=king_zone[b8];
    king_zone[h1]=king_zone[g1];
    king_zone[h8]=king_zone[g8];
  }

  u64 line(const int a,const int b){
    const int df=abs(sq_file(a)-sq_file(b));
    const int dr=abs(get_rank(a)-get_rank(b));
    if(df!=dr&&dr&&df) return 0;
    u64 bb=-1ULL<<a^-1ULL<<b;
    if(!dr||!df) bb&=piece_area[rook][a]&piece_area[rook][b];
    else bb&=piece_area[bishop][a]&piece_area[bishop][b];
    return bb;
  }

  void init_lines(){
    for(int i=0;i<board_size;i++) for(int j=0;j<board_size;j++) bline[i][j]=line(i,j);
  }

  u64 bbmask(const int piece,const int sq){
    u64 b;
    if(piece==bishop) b=piece_area[bishop][sq]&ring;
    else{
      u64 m=ring_c;
      if(get_rank(sq)==rank_1) m^=rank1;
      if(get_rank(sq)==rank_8) m^=rank8;
      if(sq_file(sq)==file_a) m^=filea;
      if(sq_file(sq)==file_h) m^=fileh;
      b=piece_area[rook][sq]&m;
    }
    return b;
  }

  u64 pdep(const u64 occ,const u64 mask){
    u64 r=0;
    u64 i=1;
    for_each_set_bit(mask,[&](const u64 bit){
      if(occ&i) r|=bb(bsf(bit));
      i<<=1;
    });
    return r;
  }

  void init_att_bitboards(){
    int i,piece;
    constexpr int pieces[]={bishop,rook,0};
    u64* att_tables[]={bishop_atts_table,rook_atts_table};
    std::array<att_lookup_t,board_size>* att_lookups[]={&bishop_att_lookup,&rook_att_lookup};
    for(int p=0;(piece=pieces[p]);p++){
      u64* att=att_tables[p];
      for(int sq=0;sq<board_size;sq++){
        const u64 mask=bbmask(piece,sq);
        att_lookup_t& att_lookup=(*att_lookups[p])[sq];
        const int pcnt=popcnt(mask);
        att_lookup.mask=mask;
        att_lookup.attacks=att;
        for(i=0;i<1<<pcnt;i++){
          const u64 occ=pdep(i,mask);
          const int j=pext(occ,mask);
          att[j]=piece_att(occ,piece,sq);
        }
        att+=i;
      }
    }
  }
}

void init_bbs(){
  init_piece_area();
  init_att_bitboards();
  init_pawn_boards();
  init_king_zone();
  init_lines();
}
