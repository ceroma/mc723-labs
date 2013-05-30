//////////////////////////////////////////////////////////////////////////////
// Standard includes
// SystemC includes
// ArchC includes

#include "ac_tlm_router.h"

//////////////////////////////////////////////////////////////////////////////

/// Namespace to isolate router from ArchC
using user::ac_tlm_router;

/// Constructor
ac_tlm_router::ac_tlm_router(sc_module_name module_name)
  : sc_module(module_name)
  , target_export("iport")
  , mem_port("mem_port", 5242880U)
  , lock(0)
{
    /// Binds target_export to the router
    target_export(*this);
}

/// Destructor
ac_tlm_router::~ac_tlm_router() {}

/**
 * Read the current value in lock and mark it as taken. A read value of 0 means
 * the lock was granted to the reading processor.
 * @param d will be 0 if lock was given to this core, other values mean taken
 * @returns A TLM response packet with SUCCESS and a modified d
 */
ac_tlm_rsp_status ac_tlm_router::read_lock(uint32_t &d)
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
ac_tlm_rsp_status ac_tlm_router::write_lock(const uint32_t &d)
{
  lock = d;
  return SUCCESS;
}
