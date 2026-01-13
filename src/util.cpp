#include "util.h"
#include <cstdio>
#include <iostream>
#include "position.h"

void sleep_ms(const int sleep_in_ms){
  Sleep(sleep_in_ms);
}

char* to_str(const move_t move){
  static char buffer[8];
  if(!is_mv(move)){
    sprintf(buffer,"(none)");
  } else{
    constexpr char promotion_codes[]={0,'n','b','r','q'};
    const int from=mv_from(move);
    const int to=mv_to(move);
    const int promo=promoted_to(move);
    sprintf(buffer,"%c%c%c%c%c",file_chr(from),rank_chr(from),
      file_chr(to),rank_chr(to),
      promotion_codes[promo]);
  }
  return buffer;
}

move_t str_to_m(const char* line) {
  const move_t move=mv(static_cast<u8>(chr_to_sq(line[0], line[1])), static_cast<u8>(chr_to_sq(line[2], line[3])));
  int piece=0;
  if (strlen(line) >= 5)
    switch (line[4]) {
    case 'q': piece=queen; break;
    case 'r': piece=rook; break;
    case 'b': piece=bishop; break;
    case 'n': piece=knight; break;
    default:;
    }
  return with_promo(move, static_cast<u8>(piece));
}

void print_board(const position_t* pos){
  int display_piece;
  SO<<SE;
  for(int i=0;i<board_size;i++){
    if(const int piece=pos->board[i];piece==empty){
      display_piece='.';
    } else{
      constexpr char display_pieces[n_pieces]={'P','N','B','R','Q','K'};
      display_piece=display_pieces[to_white(piece)];
      if(get_side(piece)==black) display_piece+=32;
    }
    SO<<' '<<SC<char>(display_piece);
    if((i&7)==7)
      SO<<SE;
  }
  SO<<"\n a b c d e f g h \n"<<SE;
}
