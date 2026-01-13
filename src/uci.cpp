#include "uci.h"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <iostream>
#include <sstream>

#include "bitboard.h"
#include "hash.h"
#include "main.h"
#include "position.h"
#include "search.h"
#include "util.h"

namespace{
  bool compare(std::istringstream& iss,const std::string& cmd){
    const std::streampos orig_pos=iss.tellg();
    std::string token;
    for(const char c:cmd){
      if(char buf_c;!(iss.get(buf_c))||buf_c!=c){
        iss.clear();
        iss.seekg(orig_pos);
        return false;
      }
    }
    if(const int next=iss.peek();next==EOF||next==' '||next=='\n'||next=='\r'){
      if(next==' ') iss.get();
      return true;
    }
    iss.clear();
    iss.seekg(orig_pos);
    return false;
  }

  void set_piece(position_t* pos,const int piece,const int sq){
    const int w_piece=to_white(piece);
    const int piece_side=get_side(piece);
    pos->board[sq]=static_cast<u8>(piece);
    pos->occ[piece_side]|=bb(static_cast<u8>(sq));
    if(w_piece==king) pos->k_sq[piece_side]=sq;
    else pos->piece_occ[w_piece]|=bb(sq);
  }

  void read_fen(search_data_t* sd,const std::string& fen_input){
    reset_search_data(sd);
    position_t* pos=sd->pos;
    for(u8& i:pos->board) i=empty;
    int sq=0;
    std::istringstream fen_stream(fen_input);
    std::string board_part,side_str,castling_str,ep_str,moves_str;
    std::getline(fen_stream,board_part,' ');
    fen_stream>>side_str>>castling_str>>ep_str;
    const char* ptr=board_part.c_str();
    char c;
    while((c=*ptr++)){
      if(c=='/') continue;
      if(c>='0'&&c<='9'){
        sq+=c-'0';
        continue;
      }
      const int side=(c>='a')?change_side:0;
      switch(c|0x60){
      case 'k': set_piece(pos,king|side,sq);
        break;
      case 'q': set_piece(pos,queen|side,sq);
        break;
      case 'r': set_piece(pos,rook|side,sq);
        break;
      case 'b': set_piece(pos,bishop|side,sq);
        break;
      case 'n': set_piece(pos,knight|side,sq);
        break;
      case 'p': set_piece(pos,pawn|side,sq);
        break;
      default: break;
      }
      sq++;
    }
    pos->side=(side_str=="w")?white:black;
    pos->c_flag=0;
    if(castling_str!="-"){
      for(const char cc:castling_str){
        switch(cc){
        case 'K': pos->c_flag|=c_flag_wr;
          break;
        case 'Q': pos->c_flag|=c_flag_wl;
          break;
        case 'k': pos->c_flag|=c_flag_br;
          break;
        case 'q': pos->c_flag|=c_flag_bl;
          break;
        default: break;
        }
      }
    }
    pos->ep_sq=n_sqs;
    if(ep_str!="-"){
      pos->ep_sq=chr_to_sq(ep_str[0],ep_str[1]);
    }
    set_phase(pos);
    reevaluate_position(pos);
    set_pins_and_checks(pos);
    std::string token;
    while(fen_stream>>token){
      if(token=="moves"){
        while(fen_stream>>token){
          make_move_rev(sd,str_to_m(token.c_str()));
        }
        break;
      }
    }
  }

  void position_startpos(const std::string& input){
    read_fen(search_config.sd,initial_fen);
    std::istringstream iss(input);
    std::string token;
    while(iss>>token){
      if(token=="moves"){
        while(iss>>token){
          make_move_rev(search_config.sd,str_to_m(token.c_str()));
        }
        break;
      }
    }
  }

  void setup_time_control(const position_t* pos){
    search_state.max_time=1<<30;
    search_state.max_depth=max_depth-1;
    if(search_state.go.infinite){} else if(search_state.go.depth>0){
      search_state.max_depth=std::min(search_state.go.depth,search_state.max_depth);
    } else{
      if(search_state.go.movetime>0) search_state.max_time=std::max(search_state.go.movetime-reduce_time,1);
      else{
        int moves_to_go=std::min(search_state.go.movestogo,static_cast<int>(max_moves_to_go));
        if(moves_to_go==0) moves_to_go=max_moves_to_go;
        const int time_reduction=std::min(search_state.go.time*reduce_time_percent/100,
          static_cast<int>(max_reduce_time))+reduce_time;
        const int max_time_allowed=std::max(search_state.go.time-time_reduction,1);
        int target_time=max_time_allowed/moves_to_go+search_state.go.inc;
        if(popcnt(pos->occ[white]&(rank1|rank2))>=11||
          popcnt(pos->occ[black]&(rank7|rank8))>=11)
          target_time=static_cast<int>((target_time+1.0)/1.5);
        for(int i=0;i<tmsteps;i++){
          const double ratio=min_time_ratio+i*(max_time_ratio-min_time_ratio)/(tmsteps-1);
          search_state.target_time[i]=std::min(ratio*target_time,static_cast<double>(max_time_allowed));
        }
        const int max_time=max_time_allowed/std::min(moves_to_go,static_cast<int>(max_time_moves_to_go))+search_state.go.inc;
        search_state.max_time=std::min(max_time,max_time_allowed);
      }
    }
  }

  void parse_go(const std::string& params){
    const position_t* pos=search_config.sd->pos;
    memset(&search_state,0,sizeof(search_state));
    std::istringstream iss(params);
    std::string t;
    while(iss>>t){
      if((t=="wtime"&&pos->side==white)||
        (t=="btime"&&pos->side==black)){
        iss>>t;
        search_state.go.time=std::stoi(t);
      } else if((t=="winc"&&pos->side==white)||
        (t=="binc"&&pos->side==black)){
        iss>>t;
        search_state.go.inc=std::stoi(t);
      } else if(t=="movestogo"){
        iss>>t;
        search_state.go.movestogo=std::stoi(t);
      } else if(t=="depth"){
        iss>>t;
        search_state.go.depth=std::stoi(t);
      } else if(t=="infinite"){
        search_state.go.infinite=1;
      } else if(t=="movetime"){
        iss>>t;
        search_state.go.movetime=std::stoi(t);
      }
    }
    setup_time_control(pos);
  }

  void set_hash(const int hash_size){
    if(hash_size<1) return;
    init_hash(hash_size);
    init_phash(1);
    reset_hash(search_config.sd);
  }

  void handle_isready(){
    SO<<"readyok"<<SE;
  }

  void handle_stop(){
    search_state.done=1;
    search_state.go.infinite=0;
  }

  void handle_uci_info(){
    SO<<"id name "<<engine<<" "<<version<<SE;
    SO<<"id author "<<author<<SE;
    SO<<"option name Hash type spin default "<<default_hash_size_in_mb<<" min 1 max "<<max_hash_size_in_mb<<SE;
    SO<<"uciok"<<SE;
  }

  void handle_newgame(){
    reset_all();
    read_fen(search_config.sd,initial_fen);
  }

  void handle_go(std::istringstream& iss) {
    std::string buf;
    std::getline(iss, buf);
    parse_go(buf);
    search(nullptr);
  }

  void handle_position(std::istringstream& iss){
    std::string token;
    iss>>token;
    if(token=="fen"){
      std::string fen;
      std::getline(iss,fen);
      if(const size_t start=fen.find_first_not_of(' ');start!=std::string::npos) fen=fen.substr(start);
      read_fen(search_config.sd,fen);
    } else if(token=="startpos"){
      std::string rest;
      std::getline(iss,rest);
      position_startpos(rest);
    }
  }

  void handle_setoption(std::istringstream& iss){
    std::string name,opt,value;
    iss>>name>>opt>>value;
    if(name=="name"&&opt=="Hash"&&value=="value"){
      int hash_size;
      iss>>hash_size;
      set_hash(hash_size);
    }
  }

  void handle_perft(std::istringstream& iss){
    int depth;
    iss>>depth;
    divide(search_config.sd,depth);
  }
}

void print_search_info(const move_t* pv_moves){
  std::ostringstream info_stream,pv_stream;
  if(const int score=search_state.score;is_mate_score(score)){
    info_stream<<"mate "<<((score>0?mate_score-score+1:-mate_score-score)/2);
  } else{
    info_stream<<"cp "<<score;
  }
  const u64 nodes=search_config.sd->nodes;
  const u64 elapsed_time=time_in_ms()-search_state.curr_time;
  SO<<"info depth "<<search_state.depth
    <<" score "<<info_stream.str()
    <<" nodes "<<nodes
    <<" time "<<elapsed_time
    <<" nps "<<(nodes*1000/(elapsed_time+1))<<" ";
  pv_stream<<"pv ";
  for(;*pv_moves;pv_moves++){
    pv_stream<<to_str(*pv_moves)<<" ";
  }
  SO<<pv_stream.str()<<SE;
}

void uci(){
  search_config.sd=SC<search_data_t*>(malloc(sizeof(search_data_t)));
  set_hash(default_hash_size_in_mb);
  reset_all();
  read_fen(search_config.sd,initial_fen);
  std::string input;
  while(std::getline(std::cin,input)){
    if(std::istringstream iss(input);
      compare(iss,"uci")){
      handle_uci_info();
    } else if(compare(iss,"isready")){
      handle_isready();
    } else if(compare(iss,"quit")){
      break;
    } else if(compare(iss,"stop")){
      handle_stop();
    } else if(compare(iss,"ucinewgame")){
      handle_newgame();
    } else if(compare(iss,"position")){
      handle_position(iss);
    } else if(compare(iss,"setoption")){
      handle_setoption(iss);
    } else if(compare(iss,"print")){
      print_board(search_config.sd->pos);
    } else if(compare(iss,"perft")){
      handle_perft(iss);
    } else if(compare(iss,"go")){
      handle_go(iss);
    }
  }
}
