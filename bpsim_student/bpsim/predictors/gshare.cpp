// gshare.cpp — Part A: global history XOR PC indexes a table of 2-bit counters.
//
// Keep ALL state in storage.h objects declared as members below (Register,
// Table, DirectMappedCache, SetAssocCache). No other member variables, statics,
// globals, containers, or heap allocation. Sizes are your choice; bpsim counts
// the bits and the total must stay within 64 Kbit (check with ./bpsim -s gshare).
#include "predictor.h"

class GsharePredictor : public BranchPredictor {
  static constexpr unsigned INDEX_BITS = 14;
  Table counters_{1u << INDEX_BITS, 2, 2, Unsigned, "counters"};
  Register ghr{6, 0, Unsigned, "ghr"};

  uint64_t calcIndex(uint64_t ghr, uint64_t pc){
    pc = pc>>2;
    uint64_t index = ghr ^ pc ;
    index = (index) & ((1u<<INDEX_BITS) -1);
    return index;
  }

 public:
  bool predict(uint64_t pc) override {
    uint64_t index_value = calcIndex(ghr.get(), pc);
    if(counters_.get(index_value) >= 2){
      return true;
    }
    else{
      return false;
    }
  }

  void update(uint64_t pc, bool taken) override {
    uint64_t index_value = calcIndex(ghr.get(), pc);
    if (taken)
    {
      counters_.inc(index_value);
    }
    else{
      counters_.dec(index_value);
    }

    //update ghr
    ghr.shiftIn(taken);
  }
};

REGISTER_PREDICTOR("gshare", GsharePredictor);
