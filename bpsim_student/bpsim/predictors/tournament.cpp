// tournament.cpp — Part A: local + global predictors; 2-bit chooser counters pick one.
//
// Keep ALL state in storage.h objects declared as members below (Register,
// Table, DirectMappedCache, SetAssocCache). No other member variables, statics,
// globals, containers, or heap allocation. Sizes are your choice; bpsim counts
// the bits and the total must stay within 64 Kbit (check with ./bpsim -s tournament).
#include "predictor.h"

class TournamentPredictor : public BranchPredictor {
  static constexpr unsigned A = 11;
  static constexpr unsigned L = 10;
  static constexpr unsigned H = 4;
  static constexpr unsigned G = 14;
  static constexpr unsigned C = 12;
  Register ghr{H, 0, Unsigned, "ghr"};

  // static constexpr unsigned INDEX_BITS = ...;

  Table LH_counter_{1u << L, 2, 2, Unsigned, "LH_counters"};
  Table local_history_{1u << A, L, 0, Unsigned, "local_history" };
  Table global_counters_{1u<<G, 2, 2, Unsigned, "global_counters"};
  Table chooser_{1u<<C, 2, 1, Unsigned, "chooser"};

  uint64_t calc_local_Index(uint64_t pc){
    pc = pc >>2;
    
    uint64_t index = pc & ((1u<<A)-1);

    return index;
  };

  bool predict_local(uint64_t pc){
    uint64_t idx = calc_local_Index(pc);
    uint64_t lh = local_history_.get(idx);
    uint64_t c = LH_counter_.get(lh);

    

    if(c >= 2){
      return true;
    }
    else{
      return false;
    }
  }

  void update_local(uint64_t pc, bool taken){
    uint64_t idx = calc_local_Index(pc);
    uint64_t lh = local_history_.get(idx);

    if(taken){
      LH_counter_.inc(lh);
    }
    else{
      LH_counter_.dec(lh);
    }

    lh = ((lh << 1) | (taken)) & ((1u << L)-1);
    local_history_.set(idx, lh);
  }

  uint64_t calc_global_Index(uint64_t ghr, uint64_t pc){
    pc = pc>>2;
    uint64_t index = ghr ^ pc ;
    index = (index) & ((1u<<G) -1);
    return index;
  }

  bool predict_global(uint64_t pc){
    uint64_t idx = calc_global_Index(ghr.get(), pc);
    uint64_t  c = global_counters_.get(idx);    

    if(c >= 2){
      return true;
    }
    else{
      return false;
    }
  }

  void update_global(uint64_t pc, bool taken){
    uint64_t idx = calc_global_Index(ghr.get(), pc);
    if(taken){
      global_counters_.inc(idx);
    }
    else{
      global_counters_.dec(idx);
    }
    
  }

  uint64_t calc_chooser_Index(uint64_t pc){
    pc = pc >> 2;

    return pc & ((1u<<C)-1);
  };

  public:
    bool predict(uint64_t pc) override {
      bool localPred= predict_local(pc);
      bool globalPred= predict_global(pc);

      uint64_t chooser= chooser_.get(calc_chooser_Index(pc));
      if(chooser>=2){
        return globalPred;
      }
      else{
        return localPred;
      }
    };

    void update(uint64_t pc, bool taken) override {
      bool localPred= predict_local(pc);
      bool globalPred= predict_global(pc);

      if(globalPred!=localPred){
        if(globalPred==taken){
          chooser_.inc(calc_chooser_Index(pc));
        }
        else{
          chooser_.dec(calc_chooser_Index(pc));
        }
      }

      update_local(pc, taken);

      update_global(pc, taken);

      ghr.shiftIn(taken);
    };
};

REGISTER_PREDICTOR("tournament", TournamentPredictor);
