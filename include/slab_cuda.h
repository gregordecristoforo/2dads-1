/*
 * slab_cuda.h
 *
 */

#include "cufft.h"
#include "cuda_types.h"
#include "2dads_types.h"
#include "cuda_array2.h"
#include "slab_config.h"
#include "output.h"
#include "diagnostics.h"
#include "diag_array.h"

class slab_cuda
{
    public:
        typedef void (slab_cuda::*rhs_fun_ptr)(uint);
        slab_cuda(slab_config);
        ~slab_cuda();

        bool init_dft();
        void finish_dft();
        void test_slab_config();

        void initialize();

        void move_t(twodads::field_k_t, uint, uint);
        void move_t(twodads::field_t, uint, uint);
        void copy_t(twodads::field_k_t, uint, uint);
        void set_t(twodads::field_k_t, cuda::cmplx_t, uint);

        // compute spectral derivative
        void d_dx(twodads::field_k_t, twodads::field_k_t, uint);
        void d_dy(twodads::field_k_t, twodads::field_k_t, uint);
        // Solve laplace equation in k-space
        void inv_laplace(twodads::field_k_t, twodads::field_k_t, uint);
        
        // Advance all fields with multiple time levels
        void advance();
        // Compute RHS function into tlev0 of theta_rhs_hat, omega_rhs_hat
        void rhs_fun(uint);
        // Compute all real fields and spatial derivatives from Fourier coeffcients at specified
        // time level
        void update_real_fields(uint);
        // Compute new theta_hat, omega_hat into tlev0.
        void integrate_stiff(twodads::dyn_field_t, uint);

        // Carry out DFT
        void dft_r2c(twodads::field_t, twodads::field_k_t, uint);
        void dft_c2r(twodads::field_k_t, twodads::field_t, uint);

        void dump_field(twodads::field_t);
        void dump_field(twodads::field_k_t);

        // Output methods
        // Make output_h5 a pointer since we deleted the default constructor
        void write_output(twodads::real_t);
        void write_diagnostics(twodads::real_t);

        void dump_address();
    private:
        slab_config config;
        const uint Nx;
        const uint My;
        const uint tlevs;

        cuda_array<cuda::real_t> theta, theta_x, theta_y;
        cuda_array<cuda::real_t> omega, omega_x, omega_y;
        cuda_array<cuda::real_t> strmf, strmf_x, strmf_y;
        cuda_array<cuda::real_t> tmp_array;

        cuda_array<cuda::cmplx_t> theta_hat, theta_x_hat, theta_y_hat;
        cuda_array<cuda::cmplx_t> omega_hat, omega_x_hat, omega_y_hat;
        cuda_array<cuda::cmplx_t> strmf_hat, strmf_x_hat, strmf_y_hat;
        cuda_array<cuda::cmplx_t> tmp_array_hat;

        cuda_array<cuda::cmplx_t> theta_rhs_hat;
        cuda_array<cuda::cmplx_t> omega_rhs_hat;

        // Arrays that store data on CPU for diagnostic functions
        diag_array<cuda::real_t> theta_diag, theta_x_diag, theta_y_diag;
        diag_array<cuda::real_t> omega_diag, omega_x_diag, omega_y_diag;
        diag_array<cuda::real_t> strmf_diag, strmf_x_diag, strmf_y_diag;
        rhs_fun_ptr theta_rhs_fun;
        rhs_fun_ptr omega_rhs_fun;

        // DFT plans
        cufftHandle plan_r2c;
        cufftHandle plan_c2r;

        bool dft_is_initialized;
        output_h5 slab_output;
        diagnostics slab_diagnostic;

        // Parameters for stiff time integration
        const cuda::stiff_params_t stiff_params;
        const cuda::slab_layout_t slab_layout;

        // Block and grid dimensions for kernels operating on Nx*My arrays.
        // For kernels where every element is treated alike
        dim3 block_nx_my;
        dim3 grid_nx_my;

        // Block and grid dimensions for arrays Nx * My/2+1
        // Row-like blocks, spanning 0..My/2 in multiples of cuda::cuda_blockdim_my
        // effectively wasting blockdim_my-1 threads in the last call. But memory is
        // coalesced :)
        dim3 block_my21_sec1;
        dim3 grid_my21_sec1;
        
        // Alternative to block_my21_sec1 would be to leave out the last column
        // and call all kernels a second time doing only the last row, as for
        // inv_lapl, integrate_stiff etc.
        // Drawback: diverging memory access and second kernel function to implement
        // new indexing
        dim3 block_my21_sec2; 
        dim3 grid_my21_sec2; 

        // Grid sizes for x derivative 
        // used to avoid if-block to compute wave number
        dim3 grid_dx_half;
        dim3 grid_dx_single;

        // Block and grid sizes for inv_lapl and integrate_stiff kernels
        dim3 block_sec12;
        dim3 grid_sec1;
        dim3 grid_sec2;

        dim3 block_sec3;
        dim3 block_sec4;
        dim3 grid_sec3;
        dim3 grid_sec4;

        cuda::real_t* d_ss3_alpha;
        cuda::real_t* d_ss3_beta;
        // Get cuda_array corresponding to field type, real field
        cuda_array<cuda::real_t>* get_field_by_name(twodads::field_t);
        // Get cuda_array corresponding to field type, complex field
        cuda_array<cuda::cmplx_t>* get_field_by_name(twodads::field_k_t);
        // Get cuda_array corresponding to output field type 
        cuda_array<cuda::real_t>* get_field_by_name(twodads::output_t);
        // Get dynamic field corresponding to field name for time integration
        cuda_array<cuda::cmplx_t>* get_field_by_name(twodads::dyn_field_t);
        // Get right-hand side for time integration corresponding to dynamic field 
        cuda_array<cuda::cmplx_t>* get_rhs_by_name(twodads::dyn_field_t);

        void theta_rhs_lin(uint);
        void theta_rhs_log(uint);
        void theta_rhs_null(uint);
        void theta_rhs_hw(uint);

        void omega_rhs_lin(uint);
        void omega_rhs_hw(uint);
        //void omega_rhs_hwmod(uint);
        void omega_rhs_null(uint);
        void omega_rhs_ic(uint);
};

// End of file slab_cuda.h
