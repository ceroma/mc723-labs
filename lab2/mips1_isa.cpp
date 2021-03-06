/**
 * @file      mips1_isa.cpp
 * @author    Sandro Rigo
 *            Marcus Bartholomeu
 *            Alexandro Baldassin (acasm information)
 *
 *            The ArchC Team
 *            http://www.archc.org/
 *
 *            Computer Systems Laboratory (LSC)
 *            IC-UNICAMP
 *            http://www.lsc.ic.unicamp.br/
 *
 * @version   1.0
 * @date      Mon, 19 Jun 2006 15:50:52 -0300
 *
 * @brief     The ArchC i8051 functional model.
 *
 * @attention Copyright (C) 2002-2006 --- The ArchC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include  "mips1_isa.H"
#include  "mips1_isa_init.cpp"
#include  "mips1_bhv_macros.H"


//If you want debug information for this model, uncomment next line
#define DEBUG_MODEL
#include "ac_debug_model.H"


//!User defined macros to reference registers.
#define Ra 31
#define Sp 29

// 'using namespace' statement to allow access to all
// mips1-specific datatypes
using namespace mips1_parms;

/**
 * A simplified representation of an instruction, with useful information for 
 * data hazard analysis.
 */
class Instruction
{
    enum Type {ALU, LOAD, NOP, OTHER};

    Type type;
    int destinationRegister;

  public:

    Instruction() {
      type = OTHER;
      destinationRegister = -1;
    }

    /**
     * Marks this is an ALU instruction.
     *
     * @param rd register being modified by the ALU instruction
     */
    void setIsALU(int rd) {
      type = ALU;
      destinationRegister = rd;
    }

    /**
     * Marks this is a load instruction.
     *
     * @param rt register being loaded by the load instruction
     */
    void setIsLoad(int rt) {
      type = LOAD;
      destinationRegister = rt;
    }

    /**
     * Marks this is a nop instruction.
     */
    void setIsNop() {
      type = NOP;
      destinationRegister = -1;
    }

    /**
     * Marks this is not a load instruction.
     */
    void setIsOther() {
      type = OTHER;
      destinationRegister = -1;
    }

    bool isALU() {
      return type == ALU;
    }

    bool isLoad() {
      return type == LOAD;
    }

    bool isNop() {
      return type == NOP;
    }

    int getDestinationRegister() {
      return destinationRegister;
    }
};

Instruction lastInstruction;
Instruction lastLastInstruction;

// Hazard counters
uint64_t num_branch_data_hazards = 0;
uint64_t num_load_use_data_hazards = 0;

/**
 * Check for a load-use data hazard.
 * There's a load-use data hazard when a register that is being loaded by a load
 * instruction has not yet become available when it is needed by another
 * instruction.
 *
 * @param rs register being used as rs by the current instruction
 * @param rt register being used as rt by the current instruction
 */
void check_load_use_hazard(int rs, int rt)
{
  int rd = lastInstruction.getDestinationRegister();
  if (lastInstruction.isLoad() && (rs == rd || rt == rd)) {
    dbg_printf("Load-Use Data Hazard!\n");
    ++num_load_use_data_hazards;
    return;
  }

  rd = lastLastInstruction.getDestinationRegister();
  if (lastInstruction.isNop() &&
      lastLastInstruction.isLoad() && (rs == rd || rt == rd)) {
    dbg_printf("Compiler-Handled Load-Use Data Hazard!\n");
    ++num_load_use_data_hazards;
    return;
  }
}

/**
 * Same as above, but to be used by I-type instructions that only use one of
 * the registers as source.
 *
 * @param rs register being used as source by the current instruction
 */
void check_load_use_hazard(int rs) {
  check_load_use_hazard(rs, -2);
}

/**
 * Check for a branch data hazard.
 * There's a branch data hazard when a comparison register is a destination of
 * a preceding ALU instruction, preceding load instruction or second preceding
 * load instruction.
 *
 * @param rs register being used as rs by the current instruction
 * @param rt register being used as rt by the current instruction
 */
void check_branch_data_hazard(int rs, int rt)
{
  // Early return
  if (!lastInstruction.isALU() &&
      !lastInstruction.isLoad() &&
      !lastLastInstruction.isLoad()) {
    return;
  }

  int rd = lastInstruction.getDestinationRegister();
  if (lastInstruction.isLoad() && (rs == rd || rt == rd)) {
    dbg_printf("Branch Data Hazard!\n");
    num_branch_data_hazards += 2;
    return;
  }

  if (lastInstruction.isALU() && (rs == rd || rt == rd)) {
    dbg_printf("Branch Data Hazard!\n");
    ++num_branch_data_hazards;
    return;
  }

  rd = lastLastInstruction.getDestinationRegister();
  if (lastLastInstruction.isLoad() && (rs == rd || rt == rd)) {
    dbg_printf("Branch Data Hazard!\n");
    ++num_branch_data_hazards;
    return;
  }
}

/**
 * Same as above, but to be used by branch instructions that only use one of
 * the registers as source.
 *
 * @param rs register being used as source by the current instruction
 */
void check_branch_data_hazard(int rs) {
  check_branch_data_hazard(rs, -2);
}

/**
 * An entry in the Branch Target Buffer.
 */
class BranchTargetBufferEntry
{
    enum State {STRONG_TAKEN, WEAK_TAKEN, WEAK_NOT_TAKEN, STRONG_NOT_TAKEN};

    State taken;
    ac_word target;

  public:

    BranchTargetBufferEntry() {
      target = 0;
      taken = WEAK_NOT_TAKEN;
    }

    /**
     * Returns the cached branch target.
     */
    ac_word getPredictedTarget() {
      return target;
    }

    /**
     * Returns a guess on whether we should branch or not.
     */
    bool getPrediction() {
      return taken == STRONG_TAKEN || taken == WEAK_TAKEN;
    }

    /**
     * Updates the guess based on whether the branch was actually taken or not.
     *
     * @param branchTaken  whether the branch was taken or not
     * @param branchTarget the branch's target address
     */
    void updatePrediction(bool branchTaken, ac_word branchTarget) {
      target = branchTarget;
      switch (taken) {
        case STRONG_TAKEN:
          taken = branchTaken ? STRONG_TAKEN : WEAK_TAKEN;
          break;
        case WEAK_TAKEN:
          taken = branchTaken ? STRONG_TAKEN : WEAK_NOT_TAKEN;
          break;
        case WEAK_NOT_TAKEN:
          taken = branchTaken ? WEAK_TAKEN : STRONG_NOT_TAKEN;
          break;
        case STRONG_NOT_TAKEN:
          taken = branchTaken ? WEAK_NOT_TAKEN : STRONG_NOT_TAKEN;
          break;
        default:
          break;
      }
    }
};

/**
 * A small cache indexed by the lower portion of the address of the branch
 * instruction that says whether the branch was recently taken or not.
 * 
 * It also stores the target address of the branch so that the PC can be updated
 * right away when prediction says branch should be taken and a new cycle just
 * for the computation of target address is avoided.
 */
class BranchTargetBuffer
{
    static const int BYTE_OFFSET = 2;

    uint32_t numIndexBits;
    uint64_t numPredictions;
    uint64_t numWrongPredictions;
    uint64_t numWrongPredictedTargets;
    std::vector<BranchTargetBufferEntry> predictions;

    uint32_t getIndex(ac_word address) {
      return (address >> BYTE_OFFSET) & ~(0xFFFFFFFF << numIndexBits);
    }

  public:

    BranchTargetBuffer(uint32_t numIndexBits)
      : numIndexBits(numIndexBits)
      , numPredictions(0)
      , numWrongPredictions(0)
      , numWrongPredictedTargets(0)
      , predictions(1 << numIndexBits)
    {}

    /**
     * Updates memory with the latest decision for this branch instruction.
     * Also, checks whether prediction would've been correct or not.
     *
     * @param address      the address of the branch instruction
     * @param branchTaken  whether the branch was taken or not
     * @param branchTarget the branch's target address
     */
    void update(ac_word address, bool branchTaken, ac_word branchTarget) {
      BranchTargetBufferEntry& entry = predictions[getIndex(address)];
      bool prediction = entry.getPrediction();
      if (prediction != branchTaken) {
        ++numWrongPredictions;
      } else if (prediction && entry.getPredictedTarget() != branchTarget) {
        ++numWrongPredictedTargets;
      }
      ++numPredictions;

      entry.updatePrediction(branchTaken, branchTarget);
    }

    uint64_t getNumPredictions() {
      return numPredictions;
    }

    /**
     * Returns the number of times the "take or not" prediction was wrong.
     */
    uint64_t getNumWrongPredictions() {
      return numWrongPredictions;
    }

    /**
     * Returns the number of times the prediction was correctly "take branch",
     * but the cached target was wrong.
     */
    uint64_t getNumWrongPredictedTargets() {
      return numWrongPredictedTargets;
    }
};

ac_word current_instruction;
// Buffer with 32 positions
BranchTargetBuffer prediction_buffer(5);

/**
 * A simple cache block without the actual data.
 */
class CacheBlock
{
    bool valid;
    uint32_t tag;

  public:

    CacheBlock() {
      tag = 0;
      valid = false;
    }

    /**
     * Saves the information about the address that was loaded into this block.
     */
    void set(uint32_t tag) {
      valid = true;
      this->tag = tag;
    }

    uint32_t getTag() {
      return tag;
    }

    bool isValid() {
      return valid;
    }
};

/**
 * A simple direct-mapped cache with variable number of word-sized blocks.
 */
class Cache
{
    static const int BYTE_OFFSET = 2;

    uint64_t readHits, readMisses;
    uint64_t writeHits, writeMisses;
    uint32_t numIndexBits, numTagBits;

    uint32_t numBlocks;
    std::vector<CacheBlock> blocks;

    /**
     * Simulates a cache access (read or write) and checks whether it would've
     * been a hit or not.
     *
     * @param address     the memory address being accessed
     * @param hitCounter  pointer to the hit counter
     * @param missCounter pointer to the miss counter
     */
    void access(
        ac_word address,
        uint64_t& hitCounter,
        uint64_t& missCounter) {
      uint32_t tag, index;

      address >>= AC_WORDSIZE - numTagBits - numIndexBits;
      tag = address >> numIndexBits;
      index = address & ~(0xFFFFFFFF << numIndexBits);

      CacheBlock& block = blocks[index];
      if (block.isValid() && block.getTag() == tag) {
        ++hitCounter;
      } else {
        ++missCounter;
        block.set(tag);
      }
    }

  public:

    Cache(uint32_t numIndexBits, uint32_t numBlockIndexBits)
      : readHits(0)
      , readMisses(0)
      , writeHits(0)
      , writeMisses(0)
      , numIndexBits(numIndexBits)
      , numTagBits(AC_WORDSIZE - numIndexBits - numBlockIndexBits - BYTE_OFFSET)
      , numBlocks(1 << numIndexBits)
      , blocks(numBlocks)
    {}

    /**
     * Simulates a cache read and checks whether it would've been a hit or not.
     *
     * @param address the memory address being read
     */
    void read(ac_word address) {
      access(address, readHits, readMisses);
    }

    /**
     * Simulates a cache write and checks whether it would've been a hit or not.
     *
     * @param address the memory address being written
     */
    void write(ac_word address) {
      access(address, writeHits, writeMisses);
    }

    /**
     * Returns the current miss-rate of this cache.
     */
    double getMissRate() {
      double hits, misses;
      hits = (double)(readHits + writeHits);
      misses = (double)(readMisses + writeMisses);
      return misses / (hits + misses);
    }
};

// 8KB cache: 256 (2^8) lines * 8 (2^3) words/block
Cache data_cache(8, 3);
// 1KB cache: 2 (2^1) lines * 128 (2^7) words/block
Cache instructions_cache(1, 7);

//!Generic instruction behavior method.
void ac_behavior( instruction )
{
  dbg_printf("----- PC=%#x ----- %lld\n", (int) ac_pc, ac_instr_counter);
  //  dbg_printf("----- PC=%#x NPC=%#x ----- %lld\n", (int) ac_pc, (int)npc, ac_instr_counter);
#ifndef NO_NEED_PC_UPDATE
  current_instruction = ac_pc;
  instructions_cache.read(ac_pc);
  ac_pc = npc;
  npc = ac_pc + 4;
#endif
};

//! Instruction Format behavior methods.
void ac_behavior( Type_R )
{
  check_load_use_hazard(rs, rt);
}

void ac_behavior( Type_I ){}

void ac_behavior( Type_J )
{
  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
}

//!Behavior called before starting simulation
void ac_behavior(begin)
{
  dbg_printf("@@@ begin behavior @@@\n");
  RB[0] = 0;
  npc = ac_pc + 4;

  // Is is not required by the architecture, but makes debug really easier
  for (int regNum = 0; regNum < 32; regNum ++)
    RB[regNum] = 0;
  hi = 0;
  lo = 0;

  num_branch_data_hazards = 0;
  num_load_use_data_hazards = 0;
}

//!Behavior called after finishing simulation
void ac_behavior(end)
{
  double miss_rate;
  dbg_printf("@@@ end behavior @@@\n");
  miss_rate = data_cache.getMissRate();
  dbg_printf("@@@ Data Cache Miss-Rate: %.2lf% @@@\n", 100 * miss_rate);
  miss_rate = instructions_cache.getMissRate();
  dbg_printf("@@@ Instructions Cache Miss-Rate: %.2lf% @@@\n", 100 * miss_rate);
  uint64_t total = prediction_buffer.getNumPredictions();
  uint64_t wrong1 = prediction_buffer.getNumWrongPredictions();
  dbg_printf("@@@ Number of Wrong Predictions: %llu/%llu @@@\n", wrong1, total);
  unsigned int wrong2 = prediction_buffer.getNumWrongPredictedTargets();
  dbg_printf(
    "@@@ Number of Right Predictions with Wrong Target: %llu/%llu @@@\n",
    wrong2,
    total
  );
  dbg_printf("@@@ Number of Control Hazards: %llu @@@\n", wrong1 + wrong2);
  dbg_printf(
    "@@@ Number of Branch Data Hazards: %llu @@@\n",
    num_branch_data_hazards
  );
  dbg_printf(
    "@@@ Number of Load-Use Data Hazards: %llu @@@\n",
    num_load_use_data_hazards
  );
}

//!Instruction lb behavior method.
void ac_behavior( lb )
{
  check_load_use_hazard(rs);

  char byte;
  dbg_printf("lb r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  ac_word addr = RB[rs] + imm;
  data_cache.read(addr);
  byte = DM.read_byte(RB[rs]+ imm);
  RB[rt] = (ac_Sword)byte ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lbu behavior method.
void ac_behavior( lbu )
{
  check_load_use_hazard(rs);

  unsigned char byte;
  dbg_printf("lbu r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  ac_word addr = RB[rs] + imm;
  data_cache.read(addr);
  byte = DM.read_byte(addr);
  RB[rt] = byte ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lh behavior method.
void ac_behavior( lh )
{
  check_load_use_hazard(rs);

  short int half;
  dbg_printf("lh r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  ac_word addr = RB[rs] + imm;
  data_cache.read(addr);
  half = DM.read_half(addr);
  RB[rt] = (ac_Sword)half ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lhu behavior method.
void ac_behavior( lhu )
{
  check_load_use_hazard(rs);

  unsigned short int  half;
  ac_word addr = RB[rs] + imm;
  data_cache.read(addr);
  half = DM.read_half(addr);
  RB[rt] = half ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lw behavior method.
void ac_behavior( lw )
{
  check_load_use_hazard(rs);

  dbg_printf("lw r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  ac_word addr = RB[rs] + imm;
  data_cache.read(addr);
  RB[rt] = DM.read(addr);
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lwl behavior method.
void ac_behavior( lwl )
{
  check_load_use_hazard(rs);

  dbg_printf("lwl r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  unsigned int addr, offset;
  ac_Uword data;

  addr = RB[rs] + imm;
  offset = (addr & 0x3) * 8;
  data_cache.read(addr & 0xFFFFFFFC);
  data = DM.read(addr & 0xFFFFFFFC);
  data <<= offset;
  data |= RB[rt] & ((1<<offset)-1);
  RB[rt] = data;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction lwr behavior method.
void ac_behavior( lwr )
{
  check_load_use_hazard(rs);

  dbg_printf("lwr r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  unsigned int addr, offset;
  ac_Uword data;

  addr = RB[rs] + imm;
  offset = (3 - (addr & 0x3)) * 8;
  data_cache.read(addr & 0xFFFFFFFC);
  data = DM.read(addr & 0xFFFFFFFC);
  data >>= offset;
  data |= RB[rt] & (0xFFFFFFFF << (32-offset));
  RB[rt] = data;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsLoad(rt);
};

//!Instruction sb behavior method.
void ac_behavior( sb )
{
  check_load_use_hazard(rs, rt);

  unsigned char byte;
  dbg_printf("sb r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  byte = RB[rt] & 0xFF;
  ac_word addr = RB[rs] + imm;
  data_cache.write(addr);
  DM.write_byte(addr, byte);
  dbg_printf("Result = %#x\n", (int) byte);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction sh behavior method.
void ac_behavior( sh )
{
  check_load_use_hazard(rs, rt);

  unsigned short int half;
  dbg_printf("sh r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  half = RB[rt] & 0xFFFF;
  ac_word addr = RB[rs] + imm;
  data_cache.write(addr);
  DM.write_half(addr, half);
  dbg_printf("Result = %#x\n", (int) half);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction sw behavior method.
void ac_behavior( sw )
{
  check_load_use_hazard(rs, rt);

  dbg_printf("sw r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  ac_word addr = RB[rs] + imm;
  data_cache.write(addr);
  DM.write(addr, RB[rt]);
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction swl behavior method.
void ac_behavior( swl )
{
  check_load_use_hazard(rs, rt);

  dbg_printf("swl r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  unsigned int addr, offset;
  ac_Uword data;

  addr = RB[rs] + imm;
  offset = (addr & 0x3) * 8;
  data = RB[rt];
  data >>= offset;
  data |= DM.read(addr & 0xFFFFFFFC) & (0xFFFFFFFF << (32-offset));
  data_cache.write(addr & 0xFFFFFFFC);
  DM.write(addr & 0xFFFFFFFC, data);
  dbg_printf("Result = %#x\n", data);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction swr behavior method.
void ac_behavior( swr )
{
  check_load_use_hazard(rs, rt);

  dbg_printf("swr r%d, %d(r%d)\n", rt, imm & 0xFFFF, rs);
  unsigned int addr, offset;
  ac_Uword data;

  addr = RB[rs] + imm;
  offset = (3 - (addr & 0x3)) * 8;
  data = RB[rt];
  data <<= offset;
  data |= DM.read(addr & 0xFFFFFFFC) & ((1<<offset)-1);
  data_cache.write(addr & 0xFFFFFFFC);
  DM.write(addr & 0xFFFFFFFC, data);
  dbg_printf("Result = %#x\n", data);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction addi behavior method.
void ac_behavior( addi )
{
  check_load_use_hazard(rs);

  dbg_printf("addi r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  RB[rt] = RB[rs] + imm;
  dbg_printf("Result = %#x\n", RB[rt]);
  //Test overflow
  if ( ((RB[rs] & 0x80000000) == (imm & 0x80000000)) &&
       ((imm & 0x80000000) != (RB[rt] & 0x80000000)) ) {
    fprintf(stderr, "EXCEPTION(addi): integer overflow.\n"); exit(EXIT_FAILURE);
  }

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction addiu behavior method.
void ac_behavior( addiu )
{
  check_load_use_hazard(rs);

  dbg_printf("addiu r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  RB[rt] = RB[rs] + imm;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction slti behavior method.
void ac_behavior( slti )
{
  check_load_use_hazard(rs);

  dbg_printf("slti r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  // Set the RD if RS< IMM
  if( (ac_Sword) RB[rs] < (ac_Sword) imm )
    RB[rt] = 1;
  // Else reset RD
  else
    RB[rt] = 0;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction sltiu behavior method.
void ac_behavior( sltiu )
{
  check_load_use_hazard(rs);

  dbg_printf("sltiu r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  // Set the RD if RS< IMM
  if( (ac_Uword) RB[rs] < (ac_Uword) imm )
    RB[rt] = 1;
  // Else reset RD
  else
    RB[rt] = 0;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction andi behavior method.
void ac_behavior( andi )
{	
  check_load_use_hazard(rs);

  dbg_printf("andi r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  RB[rt] = RB[rs] & (imm & 0xFFFF) ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction ori behavior method.
void ac_behavior( ori )
{	
  check_load_use_hazard(rs);

  dbg_printf("ori r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  RB[rt] = RB[rs] | (imm & 0xFFFF) ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction xori behavior method.
void ac_behavior( xori )
{	
  check_load_use_hazard(rs);

  dbg_printf("xori r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  RB[rt] = RB[rs] ^ (imm & 0xFFFF) ;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction lui behavior method.
void ac_behavior( lui )
{	
  check_load_use_hazard(rs);

  dbg_printf("lui r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  // Load a constant in the upper 16 bits of a register
  // To achieve the desired behaviour, the constant was shifted 16 bits left
  // and moved to the target register ( rt )
  RB[rt] = imm << 16;
  dbg_printf("Result = %#x\n", RB[rt]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rt);
};

//!Instruction add behavior method.
void ac_behavior( add )
{
  dbg_printf("add r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] + RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);
  //Test overflow
  if ( ((RB[rs] & 0x80000000) == (RB[rd] & 0x80000000)) &&
       ((RB[rd] & 0x80000000) != (RB[rt] & 0x80000000)) ) {
    fprintf(stderr, "EXCEPTION(add): integer overflow.\n"); exit(EXIT_FAILURE);
  }

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction addu behavior method.
void ac_behavior( addu )
{
  dbg_printf("addu r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] + RB[rt];
  //cout << "  RS: " << (unsigned int)RB[rs] << " RT: " << (unsigned int)RB[rt] << endl;
  //cout << "  Result =  " <<  (unsigned int)RB[rd] <<endl;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction sub behavior method.
void ac_behavior( sub )
{
  dbg_printf("sub r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] - RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);
  //TODO: test integer overflow exception for sub

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction subu behavior method.
void ac_behavior( subu )
{
  dbg_printf("subu r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] - RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction slt behavior method.
void ac_behavior( slt )
{	
  dbg_printf("slt r%d, r%d, r%d\n", rd, rs, rt);
  // Set the RD if RS< RT
  if( (ac_Sword) RB[rs] < (ac_Sword) RB[rt] )
    RB[rd] = 1;
  // Else reset RD
  else
    RB[rd] = 0;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction sltu behavior method.
void ac_behavior( sltu )
{
  dbg_printf("sltu r%d, r%d, r%d\n", rd, rs, rt);
  // Set the RD if RS < RT
  if( RB[rs] < RB[rt] )
    RB[rd] = 1;
  // Else reset RD
  else
    RB[rd] = 0;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction instr_and behavior method.
void ac_behavior( instr_and )
{
  dbg_printf("instr_and r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] & RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction instr_or behavior method.
void ac_behavior( instr_or )
{
  dbg_printf("instr_or r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] | RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction instr_xor behavior method.
void ac_behavior( instr_xor )
{
  dbg_printf("instr_xor r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = RB[rs] ^ RB[rt];
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction instr_nor behavior method.
void ac_behavior( instr_nor )
{
  dbg_printf("nor r%d, r%d, r%d\n", rd, rs, rt);
  RB[rd] = ~(RB[rs] | RB[rt]);
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction nop behavior method.
void ac_behavior( nop )
{
  dbg_printf("nop\n");

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsNop();
};

//!Instruction sll behavior method.
void ac_behavior( sll )
{
  dbg_printf("sll r%d, r%d, %d\n", rd, rs, shamt);
  RB[rd] = RB[rt] << shamt;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction srl behavior method.
void ac_behavior( srl )
{
  dbg_printf("srl r%d, r%d, %d\n", rd, rs, shamt);
  RB[rd] = RB[rt] >> shamt;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction sra behavior method.
void ac_behavior( sra )
{
  dbg_printf("sra r%d, r%d, %d\n", rd, rs, shamt);
  RB[rd] = (ac_Sword) RB[rt] >> shamt;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction sllv behavior method.
void ac_behavior( sllv )
{
  dbg_printf("sllv r%d, r%d, r%d\n", rd, rt, rs);
  RB[rd] = RB[rt] << (RB[rs] & 0x1F);
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction srlv behavior method.
void ac_behavior( srlv )
{
  dbg_printf("srlv r%d, r%d, r%d\n", rd, rt, rs);
  RB[rd] = RB[rt] >> (RB[rs] & 0x1F);
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction srav behavior method.
void ac_behavior( srav )
{
  dbg_printf("srav r%d, r%d, r%d\n", rd, rt, rs);
  RB[rd] = (ac_Sword) RB[rt] >> (RB[rs] & 0x1F);
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction mult behavior method.
void ac_behavior( mult )
{
  dbg_printf("mult r%d, r%d\n", rs, rt);

  long long result;
  int half_result;

  result = (ac_Sword) RB[rs];
  result *= (ac_Sword) RB[rt];

  half_result = (result & 0xFFFFFFFF);
  // Register LO receives 32 less significant bits
  lo = half_result;

  half_result = ((result >> 32) & 0xFFFFFFFF);
  // Register HI receives 32 most significant bits
  hi = half_result ;

  dbg_printf("Result = %#llx\n", result);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction multu behavior method.
void ac_behavior( multu )
{
  dbg_printf("multu r%d, r%d\n", rs, rt);

  unsigned long long result;
  unsigned int half_result;

  result  = RB[rs];
  result *= RB[rt];

  half_result = (result & 0xFFFFFFFF);
  // Register LO receives 32 less significant bits
  lo = half_result;

  half_result = ((result>>32) & 0xFFFFFFFF);
  // Register HI receives 32 most significant bits
  hi = half_result ;

  dbg_printf("Result = %#llx\n", result);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction div behavior method.
void ac_behavior( div )
{
  dbg_printf("div r%d, r%d\n", rs, rt);
  // Register LO receives quotient
  lo = (ac_Sword) RB[rs] / (ac_Sword) RB[rt];
  // Register HI receives remainder
  hi = (ac_Sword) RB[rs] % (ac_Sword) RB[rt];

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction divu behavior method.
void ac_behavior( divu )
{
  dbg_printf("divu r%d, r%d\n", rs, rt);
  // Register LO receives quotient
  lo = RB[rs] / RB[rt];
  // Register HI receives remainder
  hi = RB[rs] % RB[rt];

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction mfhi behavior method.
void ac_behavior( mfhi )
{
  dbg_printf("mfhi r%d\n", rd);
  RB[rd] = hi;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction mthi behavior method.
void ac_behavior( mthi )
{
  dbg_printf("mthi r%d\n", rs);
  hi = RB[rs];
  dbg_printf("Result = %#x\n", (unsigned int)hi);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction mflo behavior method.
void ac_behavior( mflo )
{
  dbg_printf("mflo r%d\n", rd);
  RB[rd] = lo;
  dbg_printf("Result = %#x\n", RB[rd]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction mtlo behavior method.
void ac_behavior( mtlo )
{
  dbg_printf("mtlo r%d\n", rs);
  lo = RB[rs];
  dbg_printf("Result = %#x\n", (unsigned int)lo);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction j behavior method.
void ac_behavior( j )
{
  dbg_printf("j %d\n", addr);
  addr = addr << 2;
#ifndef NO_NEED_PC_UPDATE
  npc =  (ac_pc & 0xF0000000) | addr;
#endif
  dbg_printf("Target = %#x\n", (ac_pc & 0xF0000000) | addr );
};

//!Instruction jal behavior method.
void ac_behavior( jal )
{
  dbg_printf("jal %d\n", addr);
  // Save the value of PC + 8 (return address) in $ra ($31) and
  // jump to the address given by PC(31...28)||(addr<<2)
  // It must also flush the instructions that were loaded into the pipeline
  RB[Ra] = ac_pc+4; //ac_pc is pc+4, we need pc+8
	
  addr = addr << 2;
#ifndef NO_NEED_PC_UPDATE
  npc = (ac_pc & 0xF0000000) | addr;
#endif
	
  dbg_printf("Target = %#x\n", (ac_pc & 0xF0000000) | addr );
  dbg_printf("Return = %#x\n", ac_pc+4);
};

//!Instruction jr behavior method.
void ac_behavior( jr )
{
  dbg_printf("jr r%d\n", rs);
  // Jump to the address stored on the register reg[RS]
  // It must also flush the instructions that were loaded into the pipeline
#ifndef NO_NEED_PC_UPDATE
  npc = RB[rs], 1;
#endif
  dbg_printf("Target = %#x\n", RB[rs]);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction jalr behavior method.
void ac_behavior( jalr )
{
  dbg_printf("jalr r%d, r%d\n", rd, rs);
  // Save the value of PC + 8(return address) in rd and
  // jump to the address given by [rs]

#ifndef NO_NEED_PC_UPDATE
  npc = RB[rs], 1;
#endif
  dbg_printf("Target = %#x\n", RB[rs]);

  if( rd == 0 )  //If rd is not defined use default
    rd = Ra;
  RB[rd] = ac_pc+4;
  dbg_printf("Return = %#x\n", ac_pc+4);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsALU(rd);
};

//!Instruction beq behavior method.
void ac_behavior( beq )
{
  check_branch_data_hazard(rs, rt);

  dbg_printf("beq r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  bool taken = RB[rs] == RB[rt];
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction bne behavior method.
void ac_behavior( bne )
{	
  check_branch_data_hazard(rs, rt);

  dbg_printf("bne r%d, r%d, %d\n", rt, rs, imm & 0xFFFF);
  bool taken = RB[rs] != RB[rt];
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction blez behavior method.
void ac_behavior( blez )
{
  check_branch_data_hazard(rs);

  dbg_printf("blez r%d, %d\n", rs, imm & 0xFFFF);
  bool taken = (RB[rs] == 0) || (RB[rs] & 0x80000000);
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target, 1;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction bgtz behavior method.
void ac_behavior( bgtz )
{
  check_branch_data_hazard(rs);

  dbg_printf("bgtz r%d, %d\n", rs, imm & 0xFFFF);
  bool taken = !(RB[rs] & 0x80000000) && (RB[rs] != 0);
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction bltz behavior method.
void ac_behavior( bltz )
{
  check_branch_data_hazard(rs);

  dbg_printf("bltz r%d, %d\n", rs, imm & 0xFFFF);
  bool taken = RB[rs] & 0x80000000;
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction bgez behavior method.
void ac_behavior( bgez )
{
  check_branch_data_hazard(rs);

  dbg_printf("bgez r%d, %d\n", rs, imm & 0xFFFF);
  bool taken = !(RB[rs] & 0x80000000);
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction bltzal behavior method.
void ac_behavior( bltzal )
{
  check_branch_data_hazard(rs);

  dbg_printf("bltzal r%d, %d\n", rs, imm & 0xFFFF);
  RB[Ra] = ac_pc+4; //ac_pc is pc+4, we need pc+8
  bool taken = RB[rs] & 0x80000000;
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	
  dbg_printf("Return = %#x\n", ac_pc+4);

  lastInstruction.setIsOther();
};

//!Instruction bgezal behavior method.
void ac_behavior( bgezal )
{
  check_branch_data_hazard(rs);

  dbg_printf("bgezal r%d, %d\n", rs, imm & 0xFFFF);
  RB[Ra] = ac_pc+4; //ac_pc is pc+4, we need pc+8
  bool taken = !(RB[rs] & 0x80000000);
  ac_word target = ac_pc + (imm << 2);
  prediction_buffer.update(current_instruction, taken, target);
  if (taken) {
#ifndef NO_NEED_PC_UPDATE
    npc = target;
#endif
    dbg_printf("Taken to %#x\n", target);
  }	
  dbg_printf("Return = %#x\n", ac_pc+4);

  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
};

//!Instruction sys_call behavior method.
void ac_behavior( sys_call )
{
  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
  dbg_printf("syscall\n");
  stop();
}

//!Instruction instr_break behavior method.
void ac_behavior( instr_break )
{
  lastLastInstruction = lastInstruction;
  lastInstruction.setIsOther();
  fprintf(stderr, "instr_break behavior not implemented.\n");
  exit(EXIT_FAILURE);
}
