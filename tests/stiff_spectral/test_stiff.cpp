/*
 * Test time integration 
 */

#include <iostream>
#include <sstream>
#include "slab_bc.h"
//#include "output.h"

using namespace std;

int main(void)
{
    constexpr size_t num_tstep{10};
    slab_config_js my_config(std::string("input_test_stiff_spectral.json"));
    const size_t order{my_config.get_tint_params(twodads::dyn_field_t::f_theta).get_tlevs()};
    stringstream fname;
    {
        slab_bc my_slab(my_config);
        my_slab.initialize();

        my_slab.invert_laplace(twodads::field_t::f_omega, twodads::field_t::f_strmf, order - 1, 0);
        my_slab.rhs(order - 2, order - 1);

        size_t tstep{0};
        for(size_t tl = 0; tl < order; tl++)
        {
            fname.str(string(""));
            fname << "test_stiff_solnum_" << my_config.get_nx() << "_a" << tl << "_t" << tstep << "_host.dat";
            utility :: print((*my_slab.get_array_ptr(twodads::field_t::f_theta)), tl, fname.str());        
        }

        // Integrate first time step
        std::cout << "Integrating: t = " << tstep << std::endl;
        tstep = 1;
        my_slab.integrate(twodads::dyn_field_t::f_theta, 1);
        my_slab.integrate(twodads::dyn_field_t::f_omega, 1);
    }
}