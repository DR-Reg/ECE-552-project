#ifndef __SP_MAC_HH__
#define __SP_MAC_HH__

/*- Sparse Multiply-ACcumulate *-/
 * Desc: TODO
 */

template <typename mac_t>
class SpMac
{
private:
    mac_t weight;
    uint64_t weight_ix; // realistically, since I only check
                        // parity, this can be a 0 or a 1!
                        // depending on whether it came from
                        // an even or odd column
public:
    SpMac()
    {
        weight = mac_t::ZERO;
        weight_ix = 0;
    }

    /* returns a*w + cin. Unlike WsMac,
     * weights must be set by caller using
     * set_weight method.
     *
     * In this case we are 'double pumping'
     * in the sense that two activations
     * are passed in, and depending on the parity
     * of the stored weight index, one or the other
     * is used. 
     */
    mac_t clock(mac_t a1, mac_t a2, mac_t cin, bool enable)
    {
        if (!enable) return mac_t::ZERO;
        mac_t a = weight_ix % 2 ? a2 : a1;
        mac_t ret;
        ret.value = a.value * weight.value + cin.value;
        return ret;
    }

    void set_weight(mac_t w, uint64_t ix)
    {
        weight = w;
        weight_ix = ix;
    }
};

#endif
