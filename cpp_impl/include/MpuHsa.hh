#ifndef __MPUHSA_HH__
#define __MPUHSA_HH__ 

#include <cstdint>
#include <cstdlib>

#include "WsMac.hh"

/*- Matrix Processing Unit -- HSA Dataflow Style *-/
 * mac_t should be a mac_t_p<b>
 *
 * MPU shaped specified a priori: NxN
 *
 * Full implementation in header to avoid nttp
 *
 * Performs matrix multiplication with the same
 * type of dataflow as a HSA module would when in MMM
 * mode. It is NOT itself part of the HSA module.
 *
 * The HSA in MMM mode works as follows:
 * - Weights are stationary in each MAC (why we use WsMac)
 * - activation flow left to right
 * - partial sums flow top to bottom
 * 
 * Then results are collected at the end (store in
 * some result array)
 */
template <typename mac_t, uint64_t N>
class MpuHsa
{
private:
    uint64_t counter;
    bool enabled[N][N];
    mac_t top_values[N]; // only used for debug
    mac_t left_values[N]; // only used for debug

    mac_t acts_sram[N][N], weights_sram[N][N];
    mac_t init_acts_sram[N][N], init_weights_sram[N][N];
    WsMac<mac_t> mac_units[N][N]; 
    mac_t right_latches[N][N];  // for left-right streaming of acts
    mac_t down_latches[N][N];  // for top-down streaming of psums
       
    mac_t result[N][N];
public:
    MpuHsa(mac_t acts_sram_p[N][N], mac_t weights_sram_p[N][N])
    {
        /* Acts needs to be transposed for this dataflow style */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
                init_acts_sram[i][j] = acts_sram_p[j][i];
        for (uint64_t i = 0; i < N; i++)
            memcpy(init_weights_sram[i], weights_sram_p[i], N*sizeof(mac_t)); 
        
        reset();
    }

    void reset()
    {
        for (uint64_t i = 0; i < N; i++)
            memcpy(acts_sram[i], init_acts_sram[i], N*sizeof(mac_t)); 
        for (uint64_t i = 0; i < N; i++)
            memcpy(weights_sram[i], init_weights_sram[i], N*sizeof(mac_t));  
        counter = 0;
        for (uint64_t i = 0; i < N; i++)
            top_values[i] = mac_t::ZERO;
        for (uint64_t j = 0; j < N; j++) 
            left_values[j] = acts_sram[j][N-1];
    }

    void clock()
    {
        std::pair<mac_t, mac_t> outputs[N][N];
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
            {
                 /* start after i+j, disable after i+j+N-1, by then passed 
                 * all values through it (ix 0,..N-1)                       
                 * */
                bool enable = (i+j)<=counter && counter < (i+j+N);
                enabled[i][j] = enable;
                if (!enable) continue;

                mac_t input_left_a, input_top_cin;
                    
                input_top_cin = i == 0 ? mac_t::ZERO : down_latches[i-1][j];
                input_left_a = j == 0 ? acts_sram[i][N-1] : right_latches[i][j-1];

                /* Weight-stationary */
                mac_t input_weight =  weights_sram[i][j];

                /* I don't simulate the weight initialisation
                 * into each PE (which should occur over multiple
                 * cycles, and ideally be pipelined with the own
                 * MPU operation...)
                 **/
                outputs[i][j] = mac_units[i][j].clock(
                    input_left_a,
                    input_weight,
                    input_top_cin,
                    enable,
                    true
                );

                /* simulate the acts 'streaming' from the left */
                if (enable)
                    /* shift the column to the right (for this row only): */
                    for (int k = N-1; k > 0; k--)
                        acts_sram[i][k] = acts_sram[i][k-1];

                /* Top values always gets 0, cin starts at 0*/
                top_values[i] = mac_t::ZERO;
                left_values[j] = acts_sram[i][N-1];  
            }

        /* Update latch values, after so no weirdness 
         * Could probably figure out a loop order to
         * do this above, but this works fine.
         * Only right to latch if enabled */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
                if (enabled[i][j])
                {
                    right_latches[i][j] = outputs[i][j].first;
                    down_latches[i][j] = outputs[i][j].second;
                    /* Last row => Output generated */
                    if (i == N - 1)
                    {
                        /* shift the row down (for this column only): */
                        for (int k = N-1; k > 0; k--)
                            result[k][j] = result[k-1][j]; 
                        result[0][j] = down_latches[i][j];
                    }
                }
        counter++;
    }

    std::string to_string()
    {
        /* Format (pla = partial latch, ala = acts latch):
         * =====================================
         * |    W1,1   | ala |    W1,2   | ala |
         * -------------------------------------
         * |    pla    |-----|     pla   | --- |
         * =====================================
         * |    W2,1   | ala |    W2,2   | ala |
         * -------------------------------------
         * |    pla    |-----|     pla   | --- |
         * ===================================== 
         * Could figure out the prealloc size... but I don't want to.
         * So I use cpp string (and inefficiently)
         * */
        std::string top_rows[N], bot_rows[N];
        std::string toptop_row;
        uint64_t max_row_width = 0;
        for (uint64_t i = 0; i < N; i++)
        {
            std::string top_row, bot_row;
            for (uint64_t j = 0; j < N; j++)
            {
                mac_t pla, ala;
                pla = down_latches[i][j];
                ala = right_latches[i][j];
                std::string w_str, ala_str, pla_str;
                ala_str = std::to_string(ala.value);
                pla_str = std::to_string(pla.value);
                w_str = !enabled[i][j] ? "Disabled" : "W="+std::to_string(weights_sram[i][j].value);
                uint64_t wwidth = w_str.length();
                uint64_t bwidth = pla_str.length();
                uint64_t twidth = std::max(std::max(wwidth, bwidth), 8LLU) + 2;
                std::string pw_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(w_str));
                std::string ppla_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(pla_str));
                top_row += "| " + pw_str + " | " + ala_str + " ";
                bot_row += "| " + ppla_str + " |" + std::string(ala_str.length()+2, '-');
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
        std::string lsep = std::string(max_l_width, ' ');
        std::string sep = lsep+std::string(max_row_width+1, '=');
        ret += lsep+toptop_row + "\n" + sep + "\n";
        for (uint64_t i = 0; i < N; i++)
            ret += " " + std::to_string(left_values[i].value) + " -> " + top_rows[i] + "|\n" + \
                    lsep + bot_rows[i] + "|\n" + sep + "\n";
        return ret;
    }
    void print_result_values()
    {
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
                std::cout << "| " << result[i][j].value << " ";
            std::cout << "|" << std::endl;
        }
    }
};
#endif
