#include <iostream>

#include "Mpu.hh"

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
    Mpu<mac_t, 2, 2> mpu = Mpu<mac_t, 2, 2>(A, B);
    std::cout << "Init" << std::endl << mpu.to_string() << std::endl;
    for (int i = 0; i < 5; i++)
    {
        mpu.clock();
        std::cout << "Clock cycle #" << i+1 << std::endl << mpu.to_string() << std::endl;
    }
    mpu.print_mac_values();
//    Mac<mac_t> my_mac;
//    mac_t a, b, cin;
//    a.value   = 4;
//    b.value   = 5;
//    cin.value = 2;
//    mac_t result = my_mac.clock(a, b, cin); 
//
//    std::cout << result.value << std::endl;
}
