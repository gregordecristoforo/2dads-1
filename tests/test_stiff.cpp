/*
 *
 * test_stiff.cpp
 *
 */

#include <iostream>
#include <include/slab_cuda.h>

using namespace std;

int main(void)
{
    slab_config my_config;
    my_config.consistency();

    slab_cuda slab(my_config);
    slab.dump_stiff_params();
    slab.initialize();
    slab.integrate_stiff_debug(twodads::field_k_t::f_theta_hat, uint(4), uint(2), uint(2));
    slab.dump_field(twodads::field_k_t::f_theta_hat);
}