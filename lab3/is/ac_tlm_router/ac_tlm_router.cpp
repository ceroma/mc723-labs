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
  , lock_port("lock_port", 4U)
  , filter_port("lock_port", 44U)
{
    /// Binds target_export to the router
    target_export(*this);
}

/// Destructor
ac_tlm_router::~ac_tlm_router() {}
