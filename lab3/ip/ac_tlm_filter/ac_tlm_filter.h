//////////////////////////////////////////////////////////////////////////////

#ifndef AC_TLM_FILTER_H_
#define AC_TLM_FILTER_H_

//////////////////////////////////////////////////////////////////////////////

// Standard includes
// SystemC includes
#include <systemc>
// ArchC includes
#include "ac_tlm_protocol.H"

//////////////////////////////////////////////////////////////////////////////

// using statements
using tlm::tlm_transport_if;

//////////////////////////////////////////////////////////////////////////////

#define FILTER_ADDRESS 0x700000
#define INDEX_TL 0x00
#define INDEX_TC 0x04
#define INDEX_TR 0x08
#define INDEX_ML 0x0C
#define INDEX_MC 0x10
#define INDEX_MR 0x14
#define INDEX_BL 0x18
#define INDEX_BC 0x1C
#define INDEX_BR 0x20
#define INDEX_TYPE 0x24
#define INDEX_RESULT 0x28

#define TYPE_MEAN  0
#define TYPE_SOBEL 1

//#define DEBUG

/// Namespace to isolate filter from ArchC
namespace user
{

/// A TLM filter
class ac_tlm_filter :
  public sc_module,
  public ac_tlm_transport_if // Using ArchC TLM protocol
{
public:
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
    ac_tlm_rsp response;
    switch (request.type) {
      case READ: // Read and calculate result
        response.status = readm(request.addr, response.data);
        break;
      case WRITE: // Write input param
	response.status = writem(request.addr, request.data);
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
  ac_tlm_filter(sc_module_name module_name);

  /**
   * Default destructor.
   */
  ~ac_tlm_filter();

private:
  uint8_t *memory;
  ac_tlm_rsp_status readm(const uint32_t &, uint32_t &);
  ac_tlm_rsp_status writem(const uint32_t &, const uint32_t &);
  int mean_filter(int, int, int, int, int, int, int, int, int);
  int sobel_filter(int, int, int, int, int, int, int, int, int);
};

};

#endif //AC_TLM_FILTER_H_
