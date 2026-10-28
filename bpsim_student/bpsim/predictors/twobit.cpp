// twobit.cpp — Part A: a table of 2-bit saturating counters, indexed by PC (bimodal).
//
// Keep ALL state in storage.h objects declared as members below (Register,
// Table, DirectMappedCache, SetAssocCache). No other member variables, statics,
// globals, containers, or heap allocation. Sizes are your choice; bpsim counts
// the bits and the total must stay within 64 Kbit (check with ./bpsim -s twobit).
#include "predictor.h"

class TwoBitPredictor : public BranchPredictor {
  static constexpr unsigned INDEX_BITS = 15;
  Table outcome_state_{1u << INDEX_BITS, 2, 2, Unsigned, "outcome_state"};

  static uint64_t index(uint64_t pc){
    return (pc>>2) & ((1u<<INDEX_BITS)-1);
  }

  public:
    bool predict(uint64_t pc) override {
      uint64_t index_value=index(pc);
      if(outcome_state_.get(index_value) >= 2){
        return true;
      }
      else{
        return false;
      }
    }

    void update(uint64_t pc, bool taken) override {
      uint64_t index_value=index(pc);
      if(taken){
        outcome_state_.inc(index_value);
      }
      else{
        outcome_state_.dec(index_value);
      }
    }
};

REGISTER_PREDICTOR("twobit", TwoBitPredictor);
