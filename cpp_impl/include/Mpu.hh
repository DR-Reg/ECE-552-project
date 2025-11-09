#ifndef __MPU_HH__
#define __MPU_HH__

#include <cstdint>
#include <string>
#include <format>
#include <cstring>

#include "Mac.hh"

/*- Matrix Processing Unit *-/
 * mac_t should be a mac_t_p<b>
 *
 * MPU 'shape' must be specified a priori:
 * Simplify:square, NxN, using template params
 *
 * Full implementation in header to avoid explicit
 * instantiation due to nttp
 *
 * Performs the matrix multiplication ACTS * WEIGHTS
 */

template <typename mac_t, uint64_t N>
class Mpu
{
private:
    uint64_t counter;
    bool enabled[N][N]; // only used for debug
    mac_t top_values[N]; // only used for debug
    mac_t left_values[N]; // only used for debug
                          //
    mac_t acts_sram[N][N], weights_sram[N][N];
    mac_t init_acts_sram[N][N], init_weights_sram[N][N];
    Mac<mac_t> mac_units[N][N];
    mac_t right_latches[N][N]; // stored at each output  (acts)
    mac_t down_latches[N][N]; // stored at each output (weight)

public:
    void print_mac_values()
    {
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
                std::cout << "| " << mac_units[i][j].get_mac().value << " ";
            std::cout << "|" << std::endl;
        }
    }
    void print_acts()
    {
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
                std::cout << acts_sram[i][j].value << " ";
            std::cout << std::endl;
        }
    }
    void print_weights()
    {
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
                std::cout << weights_sram[i][j].value << " ";
            std::cout << std::endl;
        }
    }
    Mpu(mac_t acts_sram_p[N][N], mac_t weights_sram_p[N][N])
    {
        for (uint64_t i = 0; i < N; i++)
            memcpy(init_acts_sram[i], acts_sram_p[i], N*sizeof(mac_t)); 
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
            top_values[i] = weights_sram[N-1][i];
        for (uint64_t j = 0; j < N; j++) 
            left_values[j] = acts_sram[j][N-1];
    }

    void clock()
    {
        std::pair<mac_t, mac_t> outputs[N][N];
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
            {
                mac_t input_left =  j == 0 ? acts_sram[i][N-1] : right_latches[i][j-1];
                mac_t input_top =   i == 0 ? weights_sram[N-1][j] : down_latches[i-1][j];
                
                /* start after i+j, disable after i+j+N-1, by then passed 
                 * all values through it (ix 0,..N-1)                       
                 * */
                bool enable = (i+j)<=counter && counter < (i+j+N);
                enabled[i][j] = enable;

                outputs[i][j] = mac_units[i][j].clock(input_left, input_top, enable);

                /* simulate the 'streaming' */
                if (enable)
                {
                    /* shift the column to the right (for this row only): */
                    for (int k = N-1; k > 0; k--)
                        acts_sram[i][k] = acts_sram[i][k-1];
                    /* shift the row down (for this column only): */
                    for (int k = N-1; k > 0; k--)
                        weights_sram[k][j] = weights_sram[k-1][j];
                }

                top_values[i] = weights_sram[N-1][j];
                left_values[j] = acts_sram[i][N-1]; 
            }
        /* update latch values */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
            {
                std::pair<mac_t, mac_t> output = outputs[i][j];
                right_latches[i][j] = output.first;
                down_latches[i][j]  = output.second;
            }
        counter++;
    }

    std::string to_string()
    {
        /* Format (wla = wlatch, cla = carry latch, ala = acts latch):
         * =====================================
         * |    PSUM   | ala |    PSUM   | ala |
         * -------------------------------------
         * |    wla    |-----|     wla   | --- |
         * =====================================
         * |    PSUM   | ala |    PSUM   | ala |
         * -------------------------------------
         * |    wla    |-----|     wla   | --- |
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
                mac_t wla, ala;
                wla = down_latches[i][j];
                ala = right_latches[i][j];
                std::string psum_str, ala_str, wla_str, cla_str, ocla_str;
                ala_str = std::to_string(ala.value);
                wla_str = std::to_string(wla.value);
                psum_str = !enabled[i][j] ? "Disabled" : std::to_string(mac_units[i][j].get_mac().value);
                uint64_t pwidth = psum_str.length();
                uint64_t bwidth = wla_str.length();
                uint64_t twidth = std::max(std::max(pwidth, bwidth), 8LLU) + 2;
                std::string ppsum_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(psum_str));
                std::string pwla_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(wla_str));
                top_row += "| " + ppsum_str + " | " + ala_str + " ";
                bot_row += "| " + pwla_str + " |" + std::string(ala_str.length()+2, '-');
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
};

#endif
