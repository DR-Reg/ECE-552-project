#ifndef __VPU_HH__
#define __VPU_HH__

#include "Mac.hh"
#include <cstdint>

/*- Vector Processing Unit *-/
 * mac_t should be a mac_t_p<b> 
 *
 * VPU 'shape' must be specified
 * a priori: K rows and V columns, using the template
 * parameters
 */
template <typename mac_t, uint64_t K, uint64_t N>
class Vpu
{
    private:
        /* one weight/act per mac unit */
        /* one sram block per row (K rows) */
        /* N MAC units per block (N cols) */
        mac_t weights_sram[K][V], acts_sram[K][V];
        Mac<mac_t> 
    public:
        /* Pass a pointer as probably big */
        /* memcpy it anyways inside constructor */
        Vpu(mac_t *weights_sram_p[K][V], mac_t *acts_sram_p[K][V]);
        mac_tclock();
}

#endif
