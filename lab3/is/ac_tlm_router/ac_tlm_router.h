//////////////////////////////////////////////////////////////////////////////

#ifndef AC_TLM_ROUTER_H_
#define AC_TLM_ROUTER_H_
#define LOCK_ADDRESS 0x600000

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
    // Normal memory access, just redirects to memory module
    if (request.addr != LOCK_ADDRESS) {
      return mem_port->transport(request);
    }

    // Check whether processor is trying to acquire or release the lock
    ac_tlm_rsp response;
    switch (request.type) {
      case READ: // Read (and maybe acquire) lock
        response.status = read_lock(response.data);
        break;
      case WRITE: // Write (and maybe release) lock
        response.status = write_lock(request.data);
        break;
      default:
        response.status = ERROR;
        break;
    }
    return response;
  }

  /**
   * Default constructor.
   */
  ac_tlm_router(sc_module_name module_name);

  /**
   * Default destructor.
   */
  ~ac_tlm_router();

private:
  /// Lock - released on 0, taken otherwise
  uint32_t lock;
  ac_tlm_rsp_status read_lock(uint32_t &);
  ac_tlm_rsp_status write_lock(const uint32_t &);
};

};

#endif //AC_TLM_ROUTER_H_
