//////////////////////////////////////////////////////////////////////////////

#ifndef AC_TLM_ROUTER_H_
#define AC_TLM_ROUTER_H_
#define LOCK_ADDRESS 0x600000
#define FILTER_ADDRESS 0x700000
#define FILTER_END_ADDRESS 0x700028

//////////////////////////////////////////////////////////////////////////////

// Standard includes
// SystemC includes
#include <systemc>
// ArchC includes
#include "ac_tlm_port.H"
#include "ac_tlm_protocol.H"

//////////////////////////////////////////////////////////////////////////////

// using statements
using tlm::tlm_transport_if;

//////////////////////////////////////////////////////////////////////////////

//#define DEBUG

/// Namespace to isolate router from ArchC
namespace user
{

/// A TLM router
class ac_tlm_router :
  public sc_module,
  public ac_tlm_transport_if // Using ArchC TLM protocol
{
public:
  /// Port to memory device
  ac_tlm_port mem_port;
  /// Port to lock device
  ac_tlm_port lock_port;
  /// Port to filter device
  ac_tlm_port filter_port;
  /// Exposed port with ArchC interface
  sc_export<ac_tlm_transport_if> target_export;

  /**
   * Implementation of TLM transport method that handle packets of the protocol
   * doing apropriate actions. This method must be implemented (required by
   * SystemC TLM).
   *
   * @param request a received request packet
   * @return a response packet to be sent
   */
  ac_tlm_rsp transport(const ac_tlm_req &request) {
    if (request.addr >= FILTER_ADDRESS && request.addr <= FILTER_END_ADDRESS) {
      return filter_port->transport(request);
    } else if (request.addr == LOCK_ADDRESS) {
      return lock_port->transport(request);
    } else {
      return mem_port->transport(request);
    }
  }

  /**
   * Default constructor.
   */
  ac_tlm_router(sc_module_name module_name);

  /**
   * Default destructor.
   */
  ~ac_tlm_router();
};

};

#endif //AC_TLM_ROUTER_H_
