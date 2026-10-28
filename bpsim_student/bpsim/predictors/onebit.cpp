// onebit.cpp — Part A: a table of last outcomes, indexed by low-order PC bits.
//
// Keep ALL state in storage.h objects declared as members below (Register,
// Table, DirectMappedCache, SetAssocCache). No other member variables, statics,
// globals, containers, or heap allocation. Sizes are your choice; bpsim counts
// the bits and the total must stay within 64 Kbit (check with ./bpsim -s onebit).
#include "predictor.h"

class OneBitPredictor : public BranchPredictor {
  static constexpr unsigned INDEX_BITS = 16;
  Table latest_outcomes_{1u << INDEX_BITS, 1, 1, Unsigned, "latest_outcomes"};

  static uint64_t index(uint64_t pc){
    return (pc>>2) & ((1u<<INDEX_BITS)-1);
  }

  public:
    bool predict(uint64_t pc) override {
      uint64_t index_value=index(pc);
      return latest_outcomes_.get(index_value);
    }

    void update(uint64_t pc, bool taken) override {
      uint64_t index_value=index(pc);
      latest_outcomes_.set(index_value, taken);
    }
};

REGISTER_PREDICTOR("onebit", OneBitPredictor);
