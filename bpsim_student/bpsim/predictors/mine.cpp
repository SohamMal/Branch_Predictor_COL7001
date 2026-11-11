// mine.cpp — Part B: your own predictor (perceptron, loop, TAGE-style, or a new idea).
//
// Keep ALL state in storage.h objects declared as members below (Register,
// Table, DirectMappedCache, SetAssocCache). No other member variables, statics,
// globals, containers, or heap allocation. Sizes are your choice; bpsim counts
// the bits and the total must stay within 64 Kbit (check with ./bpsim -s mine).
#include "predictor.h"

class MyPredictor : public BranchPredictor {
  // TODO: constants, e.g.  static constexpr unsigned INDEX_BITS = ...;
  // TODO: storage,   e.g.  Table counters_{1u << INDEX_BITS, 2, 1, Unsigned, "counters"};

  static constexpr unsigned BASE_Index_Bits = 14;
  static constexpr unsigned HYST_SHIFT = 1;
  // static constexpr unsigned Tagged_Index_Bits = 9;
  static constexpr unsigned NUM_Tag_Tables = 7;
  static constexpr unsigned TAGGED_LOG[] = {0, 9, 8, 9, 9, 8, 8, 8};
  // static constexpr unsigned Tagged_Rows = 1u << Tagged_Index_Bits;
  static constexpr unsigned TOTAL_TAGGED_ROWS = (1u << TAGGED_LOG[1]) + (1u << TAGGED_LOG[2]) + (1u << TAGGED_LOG[3]) + (1u << TAGGED_LOG[4]) + (1u << TAGGED_LOG[5]) + (1u << TAGGED_LOG[6]) + (1u << TAGGED_LOG[7]);
  static constexpr unsigned HIST_LEN[] = {0, 4, 7, 13, 23, 41, 73, 130};
  static constexpr unsigned TAG_BITS[] = {0, 7, 8, 8, 9, 10, 11, 12};
  static constexpr unsigned U_TICK_BITS = 18;
    // loop predictor: 32 direct-mapped entries
  static constexpr unsigned LOOP_LOG = 7;
  static constexpr unsigned LOOP_TAG_BITS = 10;
  static constexpr unsigned LOOP_ITER_BITS = 10;                      // loops up to 1023 iterations
  static constexpr unsigned LOOP_ITER_MAX = (1u << LOOP_ITER_BITS) - 1;
  static constexpr unsigned LOOP_CONF_MAX = 3;                        // 2-bit confidence
  static constexpr unsigned LOOP_AGE_MAX = 3;                         // 2-bit age

  Table base_pred_{1u << BASE_Index_Bits, 1, 1, Unsigned, "base_pred"};
  Table base_hyst_{1u << (BASE_Index_Bits - HYST_SHIFT), 1, 0, Unsigned, "base_hyst"};

  Table tagged_counters_{TOTAL_TAGGED_ROWS , 3, 0, Signed, "tagged_counters"};
  Table tagged_useful_{TOTAL_TAGGED_ROWS , 2, 0, Unsigned, "tagged_useful"};

  Table tags_t1_{1u << TAGGED_LOG[1] , TAG_BITS[1], 0, Unsigned, "tags_t1"};
  Table tags_t2_{1u << TAGGED_LOG[2] , TAG_BITS[2], 0, Unsigned, "tags_t2"};
  Table tags_t3_{1u << TAGGED_LOG[3] , TAG_BITS[3], 0, Unsigned, "tags_t3"};
  Table tags_t4_{1u << TAGGED_LOG[4] , TAG_BITS[4], 0, Unsigned, "tags_t4"};
  Table tags_t5_{1u << TAGGED_LOG[5] , TAG_BITS[5], 0, Unsigned, "tags_t5"};
  Table tags_t6_{1u << TAGGED_LOG[6] , TAG_BITS[6], 0, Unsigned, "tags_t6"};
  Table tags_t7_{1u << TAGGED_LOG[7] , TAG_BITS[7], 0, Unsigned, "tags_t7"};

  // Register global_history_{64, 0, Unsigned, "global_history"};
  Register hist0_{64, 0, Unsigned, "hist0_"};
  Register hist1_{64, 0, Unsigned, "hist1_"};
  Register hist2_{2, 0, Unsigned, "hist2_"};
  
  Register use_alt_on_na_{4, 0, Signed, "use_alt_on_na"};
  Register u_tick_{U_TICK_BITS, 0, Unsigned, "u_tick"};

  Table loop_tag_{1u << LOOP_LOG, LOOP_TAG_BITS, 0, Unsigned, "loop_tag"};
  Table loop_trip_{1u << LOOP_LOG, LOOP_ITER_BITS, 0, Unsigned, "loop_trip"};
  Table loop_iter_{1u << LOOP_LOG, LOOP_ITER_BITS, 0, Unsigned, "loop_iter"};
  Table loop_conf_{1u << LOOP_LOG, 2, 0, Unsigned, "loop_conf"};
  Table loop_age_{1u << LOOP_LOG, 2, 0, Unsigned, "loop_age"};
  Table loop_dir_{1u << LOOP_LOG, 1, 0, Unsigned, "loop_dir"};
  Register use_loop_{7, 0, Signed, "use_loop"};   // >= 0: trust a confident loop over TAGE

  

  bool histBit(unsigned i){
    if (i < 64){
      return (uint64_t(hist0_.get()) >> i) & 1;
    }  
    if (i < 128){
      return (uint64_t(hist1_.get()) >> (i - 64)) & 1;
    } 
    return (uint64_t(hist2_.get()) >> (i - 128)) & 1;
  }

  uint64_t baseIndex(uint64_t pc){
    pc= pc >> 2;

    return (pc) & ((1u << BASE_Index_Bits)-1);
  }

  uint64_t baseCounter(uint64_t idx){
    uint64_t pred = base_pred_.get(idx);
    uint64_t hyst = base_hyst_.get(idx >> HYST_SHIFT);

    return (pred <<1) | hyst;
  }

  void baseTrain(uint64_t idx, bool taken){
    uint64_t c = baseCounter(idx);
    int signed_c= c;
    if(taken){
      c = std::min( signed_c+1 , 3);
    }
    else{
      c = std::max(signed_c-1, 0);
    }

    base_pred_.set(idx, c>>1);
    base_hyst_.set(idx >> HYST_SHIFT, c&1);
  }

  uint64_t slot(uint64_t table, uint64_t row){
    uint64_t offset = 0;
    for (uint64_t k = 1; k < table; k++){
      offset += 1ull << TAGGED_LOG[k];
    }
    return offset + row;
  }

  uint64_t fold(uint64_t length, uint64_t width){
    uint64_t folded = 0;
    for (uint64_t i = 0; i < length; i++){
      if (histBit(i)){
        folded = folded ^ (1ull << (i % width));
      }
    }
    return folded;
  }

  // uint64_t fold(uint64_t history, uint64_t length, uint64_t width){
  //   history = history & ((1ull << length)-1);
  //   uint64_t folded = 0;
  //   while(history){
  //     uint64_t chunk = history & ((1ull <<width)-1);
  //     folded = folded ^ chunk;
  //     history= history >> width;
  //   }
  //   return folded;
  // }

  uint64_t taggedIndex(uint64_t pc, uint64_t table){
    uint64_t bits = TAGGED_LOG[table];
    uint64_t pc_bits = pc >> 2;
    uint64_t folded_history = fold(HIST_LEN[table], bits);
    return (folded_history ^ pc_bits ^ (pc_bits >> bits)) & ((1ull << bits) - 1);
  }

  uint64_t taggedTag(uint64_t pc, uint64_t table){
    uint64_t width = TAG_BITS[table];
    uint64_t pc_bits = pc >> 2;

    uint64_t fold_a = fold(HIST_LEN[table], width);
    uint64_t fold_b = fold(HIST_LEN[table], width - 1) << 1;

    uint64_t tag = ((pc_bits ^ fold_a ^ fold_b)) & ((1ull << width)-1); 

    return tag;
  }

  uint64_t tagGet(uint64_t table, uint64_t row){
    switch (table)
    {
    case 1:
      return tags_t1_.get(row);
    
    case 2:
      return tags_t2_.get(row);

    case 3:
      return tags_t3_.get(row);

    case 4:
      return tags_t4_.get(row);
    
    case 5:
      return tags_t5_.get(row);

    case 6:
      return tags_t6_.get(row);

    case 7:
      return tags_t7_.get(row);
    
    default:
      return 0;
    }
  }
 
  void tagSet(uint64_t table, uint64_t row, uint64_t value){
    switch (table)
    {
    case 1:
      tags_t1_.set(row, value);
      break;

    case 2:
      tags_t2_.set(row, value);
      break;

    case 3:
      tags_t3_.set(row, value);
      break;

    case 4:
      tags_t4_.set(row, value);
      break;
    
        case 5:
      tags_t5_.set(row, value);
      break;

    case 6:
      tags_t6_.set(row, value);
      break;

    case 7:
      tags_t7_.set(row, value);
      break;
    
    default:
      break;
    }
  }

  void lookup(uint64_t pc, uint64_t row[], uint64_t tag[], unsigned &provider, unsigned &alternate){
    provider=0;
    alternate=0;

    for(uint64_t t=1; t <= NUM_Tag_Tables; t++){
      row[t]=taggedIndex(pc, t);
      tag[t]=taggedTag(pc, t);
    }
    for(uint64_t t=NUM_Tag_Tables; t >0; t--){
      if(tagGet(t, row[t]) == tag[t]){
        if(provider==0){
          provider=t;
        }
        else{
          alternate=t;
          break;
        }
        
      }
    }
  }

  bool tablePrediction(unsigned table, uint64_t row[], uint64_t pc){
    if (table == 0) {
      return base_pred_.get(baseIndex(pc));
    }
    int64_t counter = tagged_counters_.get(slot(table, row[table]));
    return counter >= 0;
  }
  
  bool isNewlyAllocated(unsigned table, uint64_t row[]){
    uint64_t s = slot(table, row[table]);
    int64_t c = tagged_counters_.get(s);
    return (c == 0 || c == -1) && tagged_useful_.get(s) == 0;
  } 

  //loop
  uint64_t loopIndex(uint64_t pc){ return (pc >> 2) & ((1u << LOOP_LOG) - 1); }
  uint64_t loopTag(uint64_t pc){ return (pc >> (2 + LOOP_LOG)) & ((1u << LOOP_TAG_BITS) - 1); }

  // the entry belongs to this branch AND has seen the same trip count LOOP_CONF_MAX times
  bool loopValid(uint64_t pc){
    uint64_t i = loopIndex(pc);
    return uint64_t(loop_tag_.get(i)) == loopTag(pc) && loop_conf_.get(i) == LOOP_CONF_MAX;
  }

  // keep going in 'dir' until the count reaches the learned trip count, then exit
  bool loopPrediction(uint64_t pc){
    uint64_t i = loopIndex(pc);
    bool dir = loop_dir_.get(i);
    return (loop_iter_.get(i) == loop_trip_.get(i)) ? !dir : dir;
  }

  // TAGE's own prediction (provider, or alternate if the provider is brand new)
  bool tagePredict(uint64_t pc){
    uint64_t row[NUM_Tag_Tables+1];
    uint64_t tag[NUM_Tag_Tables+1];
    unsigned provider, alternate;
    lookup(pc, row, tag, provider, alternate);
    if (provider > 0 && isNewlyAllocated(provider, row) && use_alt_on_na_.get() >= 0){
      return tablePrediction(alternate, row, pc);
    }
    return tablePrediction(provider, row, pc);
  }

 public:
  bool predict(uint64_t pc) override {
    bool tage_pred = tagePredict(pc);
    if (loopValid(pc) && use_loop_.get() >= 0){
      return loopPrediction(pc);
    }
    return tage_pred;
  }

  void update(uint64_t pc, bool taken) override {
    uint64_t row[NUM_Tag_Tables+1];
    uint64_t tag[NUM_Tag_Tables+1];
    unsigned provider;
    unsigned alternate;
    lookup(pc, row, tag, provider, alternate);
    bool provider_pred = tablePrediction(provider, row, pc);
    bool alt_pred = tablePrediction(alternate, row, pc);
    bool newly = provider > 0 && isNewlyAllocated(provider, row);
    bool final_pred = (newly && use_alt_on_na_.get() >= 0) ? alt_pred : provider_pred;

    if (newly && provider_pred != alt_pred){
      if (alt_pred == taken) use_alt_on_na_.inc();
      else use_alt_on_na_.dec();
    }

    //usefulness
    if (provider > 0 && provider_pred != alt_pred) {
      uint64_t s = slot(provider, row[provider]);
      if (provider_pred == taken){
        tagged_useful_.inc(s);
      }
      else{
        tagged_useful_.dec(s);
      }                        
    }

    //counter training
    if (provider > 0) {
      uint64_t s = slot(provider, row[provider]);
      if (taken){
        tagged_counters_.inc(s);
      } 
      else{
        tagged_counters_.dec(s);
      }       
    } 
    else {
      baseTrain(baseIndex(pc), taken);
    }

    //allocate
    if (final_pred != taken && provider < NUM_Tag_Tables) {
      bool allocated = false;
      for (unsigned t = provider + 1; t <= NUM_Tag_Tables; t++) {
        uint64_t s = slot(t, row[t]);
        if (tagged_useful_.get(s) == 0) {
          tagSet(t, row[t], tag[t]);
          tagged_counters_.set(s, taken ? 0 : -1);
          tagged_useful_.set(s, 0);
          allocated = true;
          break;
        }
      }
      if (!allocated) {
        for (unsigned t = provider + 1; t <= NUM_Tag_Tables; t++) {
          tagged_useful_.dec(slot(t, row[t]));
        }
      }
    }

        // ---------------- loop predictor ----------------
    uint64_t li = loopIndex(pc);
    bool loop_hit = uint64_t(loop_tag_.get(li)) == loopTag(pc);
    bool loop_valid = loop_hit && loop_conf_.get(li) == LOOP_CONF_MAX;
    bool loop_pred = loopPrediction(pc);

    // referee: when loop and TAGE disagree, learn which one to trust
    if (loop_valid && loop_pred != final_pred){
      if (loop_pred == taken){
        use_loop_.inc();
        loop_age_.inc(li);            // it beat TAGE: protect this entry
      }
      else{
        use_loop_.dec();
      }
    }

    if (loop_hit){
      if (loop_valid && loop_pred != taken){
        // confident loop was wrong: trip count changed, forget and relearn
        loop_trip_.set(li, 0);
        loop_iter_.set(li, 0);
        loop_conf_.set(li, 0);
        loop_age_.set(li, 0);
      }
      else if (taken == bool(loop_dir_.get(li))){
        // still looping: count one more iteration
        if (uint64_t(loop_iter_.get(li)) == LOOP_ITER_MAX){
          loop_iter_.set(li, 0);      // too long to track: give up on this entry
          loop_conf_.set(li, 0);
          loop_age_.set(li, 0);
        }
        else{
          loop_iter_.inc(li);
        }
      }
      else{
        // loop exit: same iteration count as last time?
        if (loop_iter_.get(li) == loop_trip_.get(li)){
          loop_conf_.inc(li);
        }
        else{
          loop_trip_.set(li, loop_iter_.get(li));
          loop_conf_.set(li, 0);
        }
        loop_iter_.set(li, 0);
      }
    }
    else if (final_pred != taken){
      // TAGE missed a branch we don't track: maybe a loop exit, try to claim the slot
      if (loop_age_.get(li) == 0){
        loop_tag_.set(li, loopTag(pc));
        loop_dir_.set(li, !taken);    // the missed outcome is usually the exit
        loop_iter_.set(li, 0);
        loop_trip_.set(li, 0);
        loop_conf_.set(li, 0);
        loop_age_.set(li, LOOP_AGE_MAX);
      }
      else{
        loop_age_.dec(li);
      }
    }
    
    uint64_t t = (uint64_t(u_tick_.get()) + 1) & ((1ull << U_TICK_BITS) - 1);
    u_tick_.set(t);
    if (t == 0){
      for (uint64_t s = 0; s < TOTAL_TAGGED_ROWS; s++){
        tagged_useful_.set(s, tagged_useful_.get(s) >> 1);
      }
    }  

    bool c1 = (uint64_t(hist0_.get()) >> 63) & 1;   // hist0 bit 63 -> becomes hist1 bit 0
    bool c2 = (uint64_t(hist1_.get()) >> 63) & 1;   // hist1 bit 63 -> becomes hist2 bit 0
    hist2_.shiftIn(c2);
    hist1_.shiftIn(c1);
    hist0_.shiftIn(taken);
  }
};

REGISTER_PREDICTOR("mine", MyPredictor);
