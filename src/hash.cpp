#include "hash.h"
#include <ctime>
#include <memory>
#include <random>
#include "move.h"

namespace{
  struct hash_t{
    u64 mask=0;
    hash_data_t data{};
  };

  struct{
    std::unique_ptr<hash_t[]> items;
    u64 size=0,mask=0;
    uint32_t iter=0;
  } hash_store;

  struct phash_t{
    u64 mask=0;
    phash_data_t data{};
  };

  struct{
    std::unique_ptr<phash_t[]> items;
    u64 mask=0;
  } phash_store;

  u64 rand64(){
    static std::mt19937_64 gen(std::random_device{}());
    static std::uniform_int_distribution<u64> dis;
    return dis(gen);
  }

  void init_z_keys(){
    for(auto& position:zobkeys.positions) for(u64& j:position) j=rand64();
    for(u64& c_flag:zobkeys.c_flags) c_flag=rand64();
    zobkeys.side_flag=rand64();
  }

  int to_hash_score(int score,const int ply){
    if(score>=mate_score-max_ply) score+=ply;
    else if(score<=-mate_score+max_ply) score-=ply;
    return score;
  }

  u64 xor_data(const phash_data_t* phash_data){
    return phash_data->raw[0]^phash_data->raw[1];
  }
}

zobkeys_t zobkeys{};

int adjust_hash(int score,const int ply){
  if(score>=mate_score-max_ply) score-=ply;
  else if(score<=-mate_score+max_ply) score+=ply;
  return score;
}

hash_data_t get_hash(const search_data_t* sd){
  const hash_t* hash_item=hash_store.items.get()+(sd->hash_key&hash_store.mask);
  hash_data_t hash_data=hash_item->data;
  if((sd->hash_key^hash_item->mask)!=hash_data.raw) hash_data.raw=0;
  if(const move_t hash_move=hash_data.move;is_mv(hash_move)&&!is_pseudo_legal(sd->pos,hash_move)) hash_data.raw=0;
  return hash_data;
}

void set_hash(const search_data_t* sd,const move_t move,const int score,const int static_score,const int depth,const int ply,const int bound){
  hash_t* hash_item=hash_store.items.get()+(sd->hash_key&hash_store.mask);
  if(hash_data_t hash_data=hash_item->data;(sd->hash_key^hash_item->mask)==hash_data.raw||
    hash_data.depth<=depth||
    hash_data.iter!=hash_store.iter){
    hash_data.move=with_score(move,to_hash_score(score,ply));
    hash_data.static_eval=static_score;
    hash_data.depth=depth;
    hash_data.bound=bound;
    hash_data.iter=hash_store.iter;
    hash_item->data=hash_data;
    hash_item->mask=sd->hash_key^hash_data.raw;
  }
}

void set_hash_iteration(){
  ++hash_store.iter;
}

void reset_hash(search_data_t* sd){
  if(hash_store.items) std::fill_n(hash_store.items.get(),hash_store.size,hash_t{});
  hash_store.iter=0;
  srand(static_cast<u32>(time(nullptr)));
  init_z_keys();
  sd->hash_key=rand64();
  sd->hash_keys_cnt=0;
  sd->hash_keys[sd->hash_keys_cnt]=sd->hash_key;
}

u64 init_hash(const int size_in_mb){
  const u64 size=(static_cast<u64>(size_in_mb)<<20)/sizeof(hash_t);
  u64 rounded_size=1;
  while(rounded_size<size) rounded_size<<=1;
  hash_store.mask=rounded_size-1;
  hash_store.size=rounded_size;
  hash_store.items=std::make_unique<hash_t[]>(rounded_size);
  std::fill_n(hash_store.items.get(),rounded_size,hash_t{});
  return hash_store.size;
}

constexpr u64 phash_base_size=1ULL<<16;

int get_phash(const position_t* pos,phash_data_t* phash_data){
  const phash_t* phash_item=phash_store.items.get()+(pos->phash_key&phash_store.mask);
  *phash_data=phash_item->data;
  return (pos->phash_key^phash_item->mask)==xor_data(phash_data);
}

phash_data_t set_phash(const position_t* pos,const u64 pushed_passers,const int score_mg,const int score_eg){
  phash_data_t phash_data;
  phash_data.pushed_passers=pushed_passers;
  phash_data.score_mg=score_mg;
  phash_data.score_eg=score_eg;
  phash_t* phash_item=phash_store.items.get()+(pos->phash_key&phash_store.mask);
  phash_item->data=phash_data;
  phash_item->mask=pos->phash_key^xor_data(&phash_data);
  return phash_data;
}

void init_phash(const int mb){
  const u64 size=(2*mb-1)*phash_base_size;
  u64 rounded_size=1;
  while(rounded_size<size) rounded_size<<=1;
  phash_store.mask=rounded_size-1;
  phash_store.items=std::make_unique<phash_t[]>(rounded_size);
  std::fill_n(phash_store.items.get(),rounded_size,phash_t{});
}
