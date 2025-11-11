#ifndef __HSA_HH__
#define __HSA_HH__ 

#include <cstdint>
#include <cstdlib>

#include "WsMac.hh"

/*- FULL HSA UNIT *-/
 * mac_t should be a mac_t_p<b>
 *
 * HSA shaped specified a priori: NxN
 *
 * Full implementation in header to avoid nttp
 *
 * Performs Matrix-Matrix Multiplication (MMM)
 * Performs Matrix-Matrix Multiplication (MMM)or Matrix-Vector Multplication (MVM)
 * Performs Matrix-Matrix Multiplication (MMM)when it is clocked with the appropriate signal.
 *
 * It outputs a 'ready' signal when it is finished.
 *
 * Ideally, the mode should not be switched before the
 * 'ready' signal is finished, and if it is,
 * reset() should be called beforehand.
 *
 * The input to the constructor is the weight and
 * activation matrices, however for MVM mode,
 * the last col of the activation matrix is
 * taken as the input vector (which by internal
 * tranposition of acts matrix, throughout the code
 * below is in fact the last row).
 *
 * The HSA in MMM mode works as follows:
 * - Weights are stationary in each MAC (why we use WsMac)
 * - activation flow left to right
 * - partial sums flow top to bottom
 * - results are collected at the end, but they
 *   arrive in a disorderly fashion.
 *
 * The HSA in MVM mode works as follows:
 * - Weights are stationary in each MAC (why we use WsMac)
 * - activation vector value a_i broadcast to col
 *   i in cycle i (rest of rows are idle) 
 * - partial sums flow left to right
 * - results arrive all at once at the end.
 *
 * Notice this MVM has the broadcast/psum flow
 * flipped from VpuHsa, so that we can avoid
 * having to transpose weights matrix (since
 * it must be the same data for MVM and MMM modes)
 */
template <typename mac_t, uint64_t N>
class Hsa
{
private:
    uint64_t counter;
    bool enabled[N][N];
    mac_t top_values[N]; // only used for debug
    mac_t left_values[N]; // only used for debug

    mac_t acts_sram[N][N], weights_sram[N][N];
    mac_t init_acts_sram[N][N], init_weights_sram[N][N];
    WsMac<mac_t> mac_units[N][N]; 
    /* right_latches
     * - MMM: for left-right streaming of acts
     * - MVM: unused
     */
    mac_t right_latches[N][N];
    /* down_latches
     * - MMM/MVM: for top-down streaming of psums
     */  
    mac_t down_latches[N][N];
    mac_t result[N][N];
public:
    Hsa(mac_t acts_sram_p[N][N], mac_t weights_sram_p[N][N],
            bool MVM_enable)
    {
        /* Acts needs to be transposed for this dataflow style */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
                init_acts_sram[i][j] = acts_sram_p[j][i];
        for (uint64_t i = 0; i < N; i++)
            memcpy(init_weights_sram[i], weights_sram_p[i], N*sizeof(mac_t)); 
        
        reset(MVM_enable);
    }

    void reset(bool MVM_enable)
    {
        for (uint64_t i = 0; i < N; i++)
            memcpy(acts_sram[i], init_acts_sram[i], N*sizeof(mac_t)); 
        for (uint64_t i = 0; i < N; i++)
            memcpy(weights_sram[i], init_weights_sram[i], N*sizeof(mac_t));  
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
            {
                down_latches [i][j] = mac_t::ZERO;
                right_latches[i][j] = mac_t::ZERO;
            }
        counter = 0;
        for (uint64_t i = 0; i < N; i++)
            top_values[i] = MVM_enable ? acts_sram[N-1][i] : mac_t::ZERO;
        for (uint64_t j = 0; j < N; j++) 
            left_values[j] = MVM_enable ? mac_t::ZERO : acts_sram[j][N-1];
    }

    /* MVM_enable = false => MMM mode
     * MVM_enable = true  => MVM mode
     */
    void clock(bool MVM_enable)
    {
        std::pair<mac_t, mac_t> outputs[N][N];
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
            {
                /* MMM mode:
                 *    start after i+j, disable after i+j+N-1, by then passed 
                 *    all values through it (ix 0,..N-1)                       
                 * MVM mode:
                 *    enable col-wise, when we broadcast (i.e. cycle i => enable col i)
                 * */
                bool enable = MVM_enable ? counter == j : (i+j)<=counter && counter < (i+j+N);

                enabled[i][j] = enable;
                if (!enable) continue;

                mac_t input_left_MMM_a, input_top_MMM_cin;
                mac_t input_left_MVM_cin, input_broad_MVM_a;
                    
                input_top_MMM_cin = i == 0 ? mac_t::ZERO : down_latches[i-1][j];
                input_left_MMM_a = j == 0 ? acts_sram[i][N-1] : right_latches[i][j-1];

                input_left_MVM_cin = j == 0 ? mac_t::ZERO : right_latches[i][j-1];
                input_broad_MVM_a = acts_sram[N-1][j];

                /* Weight-stationary */
                mac_t input_weight =  weights_sram[i][j];

                /* I don't simulate the weight initialisation
                 * into each PE (which should occur over multiple
                 * cycles, and ideally be pipelined during the MMM
                 * mode, since MVM mode re-uses these weights
                 * However, due to blocking this may not be true,
                 * and weight initialisation need be pipelined in MVM
                 * mode too (not too difficult)
                 **/
                outputs[i][j] = mac_units[i][j].clock(
                    MVM_enable ? input_broad_MVM_a : input_left_MMM_a,
                    input_weight,
                    MVM_enable ? input_left_MVM_cin : input_top_MMM_cin,
                    enable,
                    true
                );

                /* simulate the acts 'streaming' from the left
                 * we only have to do this for MMM mode, since in
                 * MVM mode we broadcast act values appropriately
                 * */
                /* shift the column to the right (for this row only): */
                if (!MVM_enable && enable)
                    for (int k = N-1; k > 0; k--)
                        acts_sram[i][k] = acts_sram[i][k-1];

                top_values[i] = MVM_enable ?  acts_sram[N-1][i] : mac_t::ZERO;
                left_values[i] = MVM_enable ? mac_t::ZERO : acts_sram[i][N-1];  
            }

        /* Update latch values, after so no weirdness 
         * Could probably figure out a loop order to
         * do this above, but this works fine.
         * Only right to latch if enabled */
        for (uint64_t i = 0; i < N; i++)
            for (uint64_t j = 0; j < N; j++)
                if (enabled[i][j])
                {
                    /* Output order opposite for MVM right latches,
                     * Down latches unused for MVM
                     */
                    right_latches[i][j] = MVM_enable ? outputs[i][j].second : outputs[i][j].first;
                    down_latches[i][j] = outputs[i][j].second;
                    /* Last row => Output generated
                     * only for MMM mode as MVM stores
                     * output in latches (although, again
                     * it does not matter...)
                     * */
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

    std::string to_string_MMM()
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

    std::string to_string_MVM()
    {
        /* Format (pla = partial latch, ala = acts latch):
         * =============================
         * |    W1,1   | pla  |  W1,2  |
         * -----------------------------
         * |    W2,1   | pla  |  W2,2  |
         * =============================
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
                mac_t pla;//, ala;
                pla = down_latches[i][j];
                // ala = right_latches[i][j];
                std::string w_str, ala_str, pla_str;
                /* Quick and dirty */
                // ala_str = std::to_string(ala.value);
                ala_str = "";
                pla_str = std::to_string(pla.value);
                w_str = !enabled[i][j] ? "Disabled" : "W="+std::to_string(weights_sram[i][j].value);
                uint64_t wwidth = w_str.length();
                uint64_t bwidth = pla_str.length();
                uint64_t twidth = std::max(std::max(wwidth, bwidth), 8LLU) + 2;
                std::string pw_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(w_str));
                std::string ppla_str = std::vformat("{:^"+std::to_string(twidth)+"}", std::make_format_args(pla_str));
                top_row += "| " + pw_str + " | " + pla_str + " ";
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
        std::string lsep = std::string(max_l_width, ' ');
        std::string sep = lsep+std::string(max_row_width+1, '=');
        ret += lsep+toptop_row + "\n" + sep + "\n";
        for (uint64_t i = 0; i < N; i++)
            ret += " " + std::to_string(left_values[i].value) + " -> " + top_rows[i] + "|\n";
                    // lsep + bot_rows[i] + "|\n" + sep + "\n";
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
