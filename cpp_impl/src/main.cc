#include <iostream>

#include "mac_t.hh"
#include "Mpu.hh"
#include "MpuHsa.hh"
#include "VpuHsa.hh"
#include "Vpu.hh"
#include "Hsa.hh"
#include "SpVpu.hh"

typedef mac_t_p<8> mac_t;

int main(int argc, char **argv)
{
    mac_t A[2][2] = {
        {3, 4},
        {5, 6}
    };
    mac_t B[2][2] = {
        {7, 2},
        {1, 1}
    };
    mac_t v[2] = {
        9,
        3
    };
    // Mpu<mac_t, 2> mpu = Mpu<mac_t, 2>(A, B);
    MpuHsa<mac_t, 2> mpu = MpuHsa<mac_t, 2>(A, B);
    VpuHsa<mac_t, 2> vpu = VpuHsa<mac_t, 2>(v, A);
    Hsa<mac_t, 2> hsa = Hsa<mac_t, 2>(A, B, false);
    SpVpu<mac_t, 2> hsa = SpVpu<mac_t, 2>(A, B, false);

    std::cout << "Init" << std::endl << vpu.to_string() << std::endl;
    for (int i = 0; i < 5; i++)
    {
        vpu.clock();
        std::cout << "Clock cycle #" << i+1 << std::endl << vpu.to_string() << std::endl;
    }
    // mac_t *res = vpu.get_result(); 
    // std::cout << res[0] << "\n" << res[1] << "\n";
    // free(res);

    // std::cout << "Init" << std::endl << hsa.to_string_MMM() << std::endl;
    // for (int i = 0; i < 5; i++)
    // {
    //     hsa.clock(false);
    //     std::cout << "Clock cycle #" << i+1 << std::endl << hsa.to_string_MMM() << std::endl;
    // }
    // hsa.print_result_values();
    // mpu.print_result_values();
    // mpu.print_mac_values(); 

    hsa.reset(true);
    std::cout << "Init" << std::endl << hsa.to_string_MVM() << std::endl;
    for (int i = 0; i < 5; i++)
    {
        hsa.clock(true);
        std::cout << "Clock cycle #" << i+1 << std::endl << hsa.to_string_MVM() << std::endl;
    }
}
