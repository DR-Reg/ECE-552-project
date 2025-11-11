#ifndef __SP_MPU_HH__
#define __SP_MPU_HH__

/*- Sparse Vector Processing Unit *-/
 * Desc: TODO
 */
template <typename mac_t, uint64_t N>
class SpVpu
{
private:
    uint64_t counter;
    bool enabled[N][N];
    
    std::pair<mac_t, mac_t> top_values[N]; // only used for debug, represent nothing (Cin==0 for top)
    mac_t left_values[N]; // only used for debug, represent broadcast

    mac_t acts_sram[N], weights_sram[N][N];
    uint64_t weight_tags_sram[N][N];
    mac_t init_acts_sram[N], init_weights_sram[N][N];
    uint64_t PN = N >> 1;       // By packing/double pump only need half columns
    SpMac<mac_t> mac_units[N][N>>1]; 
    mac_t right_latches[N][N];  // for left-right streaming of psums
                        

public:
    SpVpu(mac_t acts_sram_p[N], mac_t weights_sram_p[N][N], uint64_t weight_tags_sram_p[N][N])
    {
        for (uint64_t i = 0; i < N; i++)
            init_acts_sram[i] = acts_sram_p[i];
        /* Weights need NOT be transposed for this dataflow style
         * since we broadcast columnwise
         * */
        for (uint64_t i = 0; i < N; i++)
        {
            memcpy(init_weights_sram[i],  weights_sram_p[i], N*sizeof(mac_t));
            memcpy(init_weight_tags_sram[i],  weight_tags_sram_p[i], N*sizeof(uint64_t));
        }

        reset();
    }

    void reset()
    {
        for (uint64_t i = 0; i < N; i++)
            acts_sram[i] =  init_acts_sram[i];
        for (uint64_t i = 0; i < N; i++)
        {
            memcpy(weights_sram[i], init_weights_sram[i], N*sizeof(mac_t));  
            memcpy(weight_tags_sram[i], init_weight_tags_sram[i], N*sizeof(uint64_t));   
        }
        counter = 0;
        for (uint64_t i = 0; i < N; i++)
            top_values[i] = std::make_pair(mac_t::ZERO, mac_t::ZERO);
        for (uint64_t j = 0; j < N; j++) 
            left_values[j] = acts_sram[j];
    }

    void clock()
    {
        mac_t outputs[N][N];
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < PN; j++)  // recall packing and double pump along x
            {
                /* We operate a packed column at a time,
                 * so in cycle 1, column 1 enabled (double pump)
                 * cycle 2 column 2...
                 * */
                bool enable = counter==j;
                enabled[i][j] = enable;
                if (!enable)
                {
                    left_values[i] = mac_t::ZERO;
                    continue;
                }

                /* The same activation inputs (a[2*j], a[2*j+1] - double pump)
                 * should be broadcast to all the
                 * macs (we broadcast column wise,
                 * but this is fine because we skip
                 * non-enabled macs above and we enable
                 * column-wise
                 * */
                mac_t input_broad_a1, input_broad_a2, input_left_cin;
                    
                input_left_cin = j == 0 ? mac_t::ZERO : right_latches[i][j-1];
                input_broad_a1 = acts_sram[2*j];
                input_broad_a2 = acts_sram[2*j + 1];

                /* Weight-stationary */
                mac_t input_weight =  weights_sram[i][j];

                /* I don't simulate the weight initialisation
                 * into each PE (which should occur over multiple
                 * cycles, and ideally be pipelined with the own
                 * VPU operation - which is way easier in MVM
                 * mode than MMM)
                 **/
                /* 'Fake' initialisation here - in reality should
                 * be done only once
                 */
                mac_units[i][j].set_weights(
                        weights_sram[i][j],
                        weight_tags_sram[i][j]
                );
                outputs[i][j] = mac_units[i][j].clock(
                    input_broad_a1,
                    input_broad_a2,
                    input_left_cin,
                    enable
                );

                /* Left values always gets 0, cin starts at 0*/
                left_values[i] = mac_t::ZERO;
                top_values[j] = std::make_pair(acts_sram[2*j], acts_sram[2*j+1]);  
            }

        /* Update latch values, after so no weirdness 
         * Could probably figure out a loop order to
         * do this above, but this works fine.
         * Only right to latch if enabled */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < PN; j++)
                if (enabled[i][j])
                {
                    /* results latched right
                     **/
                    right_latches[i][j] = outputs[i][j];
                }
        counter++;
    }

    std::string to_string()
    {
        /* Format (pla = partial latch, wix = weight index):
         * =================================================
         * |    W1,1, WIX   | pla |   W1,2, Wix    |  pla  |
         * =================================================
         * |    W1,1, WIX   | pla |   W1,2, Wix    |  pla  |
         * ================================================= 
         * Could figure out the prealloc size... but I don't want to.
         * So I use cpp string (and inefficiently)
         * */
        std::string top_rows[N], bot_rows[N];
        std::string toptop_row;
        uint64_t max_row_width = 0;
        for (uint64_t i = 0; i < N; i++)
        {
            std::string top_row, bot_row;
            for (uint64_t j = 0; j < PN; j++)
            {
                mac_t pla;//, ala;
                pla = down_latches[i][j];
                // ala = right_latches[i][j];
                std::string w_str, ala_str, pla_str;
                /* Quick and dirty */
                // ala_str = std::to_string(ala.value);
                ala_str = "";
                pla_str = std::to_string(pla.value);
                w_str = !enabled[i][j] ? "Disabled" : "W="+std::to_string(weights_sram[i][j].value)+",ix="+std::to_string(j);
                uint64_t wwidth = w_str.length();
                uint64_t bwidth = pla_str.length();
                uint64_t twidth = std::max(std::max(wwidth, bwidth), 8LLU) + 2;
                std::string pw_str = std::vformat("{:^"+std::to_string(wwidth)+"}", std::make_format_args(w_str));
                std::string ppla_str = std::vformat("{:^"+std::to_string(bwidth)+"}", std::make_format_args(pla_str));
                top_row += "| " + pw_str + " | " + ppla_str + " ";
                // bot_row += "| " + ppla_str + " |" + std::string(ala_str.length()+2, '-');
                if (i == 0)
                {
                    std::string toptop_str = std::to_string(top_values[j].value) + "↓";
                    std::string ptoptop_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(toptop_str));
                    toptop_row += "  " + ptoptop_str + "   " + std::string(ala_str.length(), ' ') + " ";
                }
            }
            top_rows[i] = top_row;
            bot_rows[i] = bot_row;
            max_row_width = top_row.length() > max_row_width ? top_row.length() : max_row_width;
        }
        std::string ret;
        uint64_t max_l_width = 0;
        for (uint64_t i = 0; i < N; i++)
            max_l_width = std::max(max_l_width, (uint64_t)std::to_string(left_values[i].value).length());
        max_l_width += 5;
        // std::string lsep = std::string(max_l_width, ' ');
        // std::string sep = lsep+std::string(max_row_width+1, '=');
        std::string sep = " "+std::string(max_row_width+1, '=');
        ret += lsep+toptop_row + "\n" + sep + "\n";
        for (uint64_t i = 0; i < N; i++)
            ret += " " + top_rows[i] + "|\n" + sep + "\n"
            // ret += " " + std::to_string(left_values[i].value) + " -> " + top_rows[i] + "|\n" + \
                    lsep + bot_rows[i] + "|\n" + sep + "\n";
        return ret;
    }
 
};


#endif
