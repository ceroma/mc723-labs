//////////////////////////////////////////////////////////////////////////////
// Standard includes
// SystemC includes
// ArchC includes

#include "ac_tlm_filter.h"

//////////////////////////////////////////////////////////////////////////////

/// Namespace to isolate filter from ArchC
using user::ac_tlm_filter;

/// Constructor
ac_tlm_filter::ac_tlm_filter(sc_module_name module_name)
  : sc_module(module_name) 
  , target_export("iport")
{
    int k;

    /// Binds target_export to the filter
    target_export(*this);

    /// Initialize memory vector
    memory = new uint8_t[40];
    for (k = 39; k > 0; k--) memory[k] = 0;
}

/// Destructor
ac_tlm_filter::~ac_tlm_filter()
{
  delete [] memory;
}

/**
 * Calculate filtered pixel value and return the read address in d.
 * Note: Always read 32 bits
 * @param a is the address to read
 * @param d will contain the read data
 * @returns A TLM response packet with SUCCESS and a modified d
 */
ac_tlm_rsp_status ac_tlm_filter::readm(const uint32_t &a, uint32_t &d)
{
  uint32_t index = a - FILTER_ADDRESS;
  int *result, *tl, *tc, *tr, *ml, *mc, *mr, *bl, *bc, *br;

  // Apply filter
  if (index == INDEX_RESULT) {
    tl = (int *) &memory[INDEX_TL];
    tc = (int *) &memory[INDEX_TC];
    tr = (int *) &memory[INDEX_TR];
    ml = (int *) &memory[INDEX_ML];
    mc = (int *) &memory[INDEX_MC];
    mr = (int *) &memory[INDEX_MR];
    bl = (int *) &memory[INDEX_BL];
    bc = (int *) &memory[INDEX_BC];
    br = (int *) &memory[INDEX_BR];
    result = (int *) &memory[INDEX_RESULT];
    *result = mean_filter(*tl, *tc, *tr, *ml, *mc, *mr, *bl, *bc, *br);
  }

  // Flip endianness
  ((uint8_t *)&d)[0] = memory[index+3];
  ((uint8_t *)&d)[1] = memory[index+2];
  ((uint8_t *)&d)[2] = memory[index+1];
  ((uint8_t *)&d)[3] = memory[index];

  return SUCCESS;
}

/**
 * Write parameter (pixel value) to memory.
 * Note: Always write 32 bits
 * @param a is the address to write
 * @param d is the data being written
 * @returns A TLM response packet with SUCCESS
 */
ac_tlm_rsp_status ac_tlm_filter::writem(const uint32_t &a, const uint32_t &d)
{
  uint32_t index = a - FILTER_ADDRESS;

  // Flip endianness
  memory[index]   = ((uint8_t *) &d)[3];
  memory[index+1] = ((uint8_t *) &d)[2];
  memory[index+2] = ((uint8_t *) &d)[1];
  memory[index+3] = ((uint8_t *) &d)[0];

  return SUCCESS;
}

/**
 * Apply the mean filter on a 3x3 window.
 * @param t_ top pixel
 * @param m_ midle pixel
 * @param b_ bottom pixel
 * @param _l left pixel
 * @param _c center pixel
 * @param _r right pixel
 */
int ac_tlm_filter::mean_filter(int tl, int tc, int tr,
                               int ml, int mc, int mr,
                               int bl, int bc, int br) {
  return (tl + tc + tr + ml + mc + mr + bl + bc + br) / 9;
}
