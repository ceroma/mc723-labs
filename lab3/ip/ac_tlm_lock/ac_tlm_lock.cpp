//////////////////////////////////////////////////////////////////////////////
// Standard includes
// SystemC includes
// ArchC includes

#include "ac_tlm_lock.h"

//////////////////////////////////////////////////////////////////////////////

/// Namespace to isolate lock from ArchC
using user::ac_tlm_lock;

/// Constructor
ac_tlm_lock::ac_tlm_lock(sc_module_name module_name)
  : sc_module(module_name)
  , target_export("iport")
  , lock(0)
{
    /// Binds target_export to the lock
    target_export(*this);
}

/// Destructor
ac_tlm_lock::~ac_tlm_lock() {}

/**
 * Read the current value in lock and mark it as taken. A read value of 0 means
 * the lock was granted to the reading processor.
 * @param d will be 0 if lock was given to this core, other values mean taken
 * @returns A TLM response packet with SUCCESS and a modified d
 */
ac_tlm_rsp_status ac_tlm_lock::read_lock(uint32_t &d)
{
  d = lock;
  lock = 1;
  return SUCCESS;
}

/**
 * Write the value to lock. If value is 0, it means the processor is releasing
 * the lock.
 * @param d the data being written to the lock
 * @returns A TLM response packet with SUCCESS
 */
ac_tlm_rsp_status ac_tlm_lock::write_lock(const uint32_t &d)
{
  lock = d;
  return SUCCESS;
}
