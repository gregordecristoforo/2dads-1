///
/// Two-dimensional slab consisting of cuda_arrays for all variables
/// Dynamic variables are omega and theta
///


#ifndef SLAB_CUDA_H
#define SLAB_CUDA_H

#include "cufft.h"
#include "cuda_types.h"
#include "2dads_types.h"
#include "cuda_array3.h"
#include "slab_config.h"
#include "initialize.h"
#include "cufft.h"
#include <string>
#include <fstream>



class slab_cuda
{
    public:
        typedef void (slab_cuda::*rhs_fun_ptr)(uint);
        typedef cuda_array<cuda::cmplx_t, cuda::real_t> cuda_arr_cmplx;
        typedef cuda_array<cuda::real_t, cuda::real_t> cuda_arr_real;
        slab_cuda(slab_config); ///< Standard constructors
        ~slab_cuda();

        bool init_dft();  ///< Initialize cuFFT
        void finish_dft(); ///< Clean-up for cuFFT
        void test_slab_config(); ///< Test if the slab_configuration gives a good slab

        void initialize(); ///< Initialize fields in a consistent manner. See docstring of the function

        void move_t(twodads::field_k_t, uint, uint); ///< move data for specified field from t_src to t_dst
        //void move_t(twodads::field_t, uint, uint); ///< move data for real field form t_src to t_dst
        void copy_t(twodads::field_k_t, uint, uint); ///< copy data for fourier field from t_src to t_dst
        void set_t(twodads::field_k_t, cuda::cmplx_t, uint); ///< set fourier field to fixed value at given tlev
        void set_t(twodads::field_t, cuda::real_t); ///< set real field to fixed value at given tlev

        // compute spectral derivative
        void d_dx(twodads::field_k_t, twodads::field_k_t, uint); ///< Compute x derivative 
        void d_dy(twodads::field_k_t, twodads::field_k_t, uint); ///< Compute y derivative
        // Solve laplace equation in k-space
        void inv_laplace(twodads::field_k_t, twodads::field_k_t, uint); ///< Invert Laplace operators

        // Debug functions that only enumerate the array by row and col number (1000 * col + row)
        void d_dx_enumerate(twodads::field_k_t, twodads::field_k_t, uint); ///< Compute x derivative 
        void d_dy_enumerate(twodads::field_k_t, twodads::field_k_t, uint); ///< Compute y derivative
        // Solve laplace equation in k-space
        void inv_laplace_enumerate(twodads::field_k_t, twodads::field_k_t, uint); ///< Invert Laplace operators
        
        // Advance all fields with multiple time levels
        void advance(); ///<Advance all member fields with multiple time levels: theta_hat, omega_hat, theta_rhs_hat and omega_rhs_hat
        // Compute RHS function into tlev0 of theta_rhs_hat, omega_rhs_hat
        void rhs_fun(uint); ///< Call RHS_fun pointers
        // Compute all real fields and spatial derivatives from Fourier coeffcients at specified
        // time level
        void update_real_fields(uint); ///< Update all real fields
        // Compute new theta_hat, omega_hat into tlev0.
        void integrate_stiff(twodads::field_k_t, uint); ///< Time step
        void integrate_stiff_ky0(twodads::field_k_t, uint); ///< Time integration of modes with ky=0
        void integrate_stiff_enumerate(twodads::field_k_t, uint); ///< Only enumerate modes
        void integrate_stiff_debug(twodads::field_k_t, uint, uint, uint); ///< Integrate, with full debugging output
        // Carry out DFT
        void dft_r2c(twodads::field_t, twodads::field_k_t, uint); ///< Real to complex DFT
        void dft_c2r(twodads::field_k_t, twodads::field_t, uint); ///< Complex to real DFT

        void print_field(twodads::field_t); ///< Print member cuda_array to terminal
        void print_field(twodads::field_t, string); ///< Print member cuda_array to ascii file
        void print_field(twodads::field_k_t); ///< Print member cuda_aray to terminal
        void print_field(twodads::field_k_t, string); ///< Print member cuda_array<cmplx_t> to ascii file

        void get_data(twodads::field_t, cuda::real_t* buffer); ///< Export real field data into a buffer
        void print_address(); ///< Print the addresses of member variables in host memory
        void print_grids(); ///< Print the grid sizes used for cuda kernel calls

        // Output methods
        friend class output_h5;
        friend class diagnostics;

    private:
        slab_config config; ///< slab configuration
        const uint Nx; ///< Number of discretization points in x-direction
        const uint My; ///< Number of discretization points in y=direction
        const uint tlevs; ///< Number of time levels stored, order of time integration + 1

        cuda_array<cuda::real_t, cuda::real_t> theta, theta_x, theta_y; ///< Real fields assoc. with theta
        cuda_array<cuda::real_t, cuda::real_t> omega, omega_x, omega_y; ///< Real fields assoc. with omega
        cuda_array<cuda::real_t, cuda::real_t> strmf, strmf_x, strmf_y; ///< Real fields assoc.with stream function
        cuda_array<cuda::real_t, cuda::real_t> tmp_array; ///< temporary data
        cuda_array<cuda::real_t, cuda::real_t> theta_rhs, omega_rhs; ///< Real fields for RHS, for debugging

        cuda_array<cuda::cmplx_t, cuda::real_t> theta_hat, theta_x_hat, theta_y_hat; ///< Fourier fields assoc. with theta
        cuda_array<cuda::cmplx_t, cuda::real_t> omega_hat, omega_x_hat, omega_y_hat; ///< Fourier fields assoc. with omega
        cuda_array<cuda::cmplx_t, cuda::real_t> strmf_hat, strmf_x_hat, strmf_y_hat; ///< Fourier fields assoc. with strmf
        cuda_array<cuda::cmplx_t, cuda::real_t> tmp_array_hat; ///< Temporary data, Fourier field

        cuda_array<cuda::cmplx_t, cuda::real_t> theta_rhs_hat; ///< non-linear RHS for time integration of theta
        cuda_array<cuda::cmplx_t, cuda::real_t> omega_rhs_hat; ///< non-linear RHS for time integration of omea

        // Arrays that store data on CPU for diagnostic functions
        rhs_fun_ptr theta_rhs_fun; ///< Evaluate explicit part for time integrator
        rhs_fun_ptr omega_rhs_fun; ///< Evaluate explicit part for time integrator

        // DFT plans
        cufftHandle plan_r2c; ///< Plan used for all D2Z DFTs used by cuFFT
        cufftHandle plan_c2r; ///< Plan used for all Z2D iDFTs used by cuFFT

        bool dft_is_initialized; ///< True if cuFFT is initialized
        //output_h5 slab_output; ///< Handels internals of output to hdf5
        //diagnostics slab_diagnostic; ///< Handles internals of diagnostic output

        // Parameters for stiff time integration
        const cuda::stiff_params_t stiff_params; ///< Coefficients for time integration routine
        const cuda::slab_layout_t slab_layout; ///< Slab layout passed to cuda kernels and initialization routines

        ///@brief Block and grid dimensions for kernels operating on My*Nx arrays.
        ///@brief For kernels where every element is treated alike
        dim3 block_my_nx;
        dim3 grid_my_nx;

        ///@ brief Block and grid dimensions for kernels operating on My * Nx/2+1 arrays
        dim3 block_my_nx21;
        dim3 grid_my_nx21;

        /// @brief Block and grid dimensions for arrays My * Nx/2+1
        /// @brief Row-like blocks, spanning 0..Nx/2 in multiples of cuda::blockdim_nx
        /// @brief effectively wasting blockdim_nx-1 threads in the last call. But memory is
        /// @brief coalesced :)
       
        /// @brief 
        dim3 block_nx21; ///< blocksize is (cuda::blockdim_nx, 1)
        dim3 grid_nx21_sec1; ///< Sector 1: ky > 0, kx < Nx / 2
        dim3 grid_nx21_sec2; ///< Sector 2: ky < 0, kx < Nx / 2
        dim3 grid_nx21_sec3; ///< Sector 3: ky > 0, kx = Nx / 2
        dim3 grid_nx21_sec4; ///< Sector 4: ky < 0, kx = Nx / 2

        dim3 grid_dx_half; ///< All modes with kx < Nx / 2
        dim3 grid_dx_single; ///< All modes with kx = Nx / 2

        dim3 grid_dy_half; ///< My/2 rows, all columns
        dim3 grid_dy_single; ///< 1 row, all columns


        /// @brief Grid dimensions for access on all {kx, 0} modes, stored in the first Nx/2+1 elements of an array
        dim3 grid_ky0;

        cuda::real_t* d_ss3_alpha; ///< Coefficients for implicit part of time integration
        cuda::real_t* d_ss3_beta; ///< Coefficients for explicit part of time integration
        /// Get pointer to cuda_array<cuda::real_t> corresponding to field type 
        cuda_array<cuda::real_t, cuda::real_t>* get_field_by_name(twodads::field_t);
        /// Get pointer to cuda_array<cuda::cmplx_t> corresponding to field type 
        cuda_array<cuda::cmplx_t, cuda::real_t>* get_field_by_name(twodads::field_k_t);
        /// Get pointer to cuda_array<cuda::real_t> corresponding to output field type 
        cuda_array<cuda::real_t, cuda::real_t>* get_field_by_name(twodads::output_t);
        /// Get pointer to cuda_array<cuda::real_t> corresponding to dynamic field type 
        //cuda_array<cuda::cmplx_t, cuda::real_t>* get_field_by_name(twodads::field_k_t);
        /// Get pointer to cuda_array<cuda::real_t> corresponding to RHS field type 
        cuda_array<cuda::cmplx_t, cuda::real_t>* get_rhs_by_name(twodads::field_k_t);

        void theta_rhs_ns(uint); ///< Navier-Stokes 
        void theta_rhs_lin(uint); ///< Small amplitude blob
        void theta_rhs_log(uint); ///< Arbitrary amplitude blob
        void theta_rhs_null(uint); ///< Zero explicit term
        void theta_rhs_hw(uint); ///< Hasegawa-Wakatani model 
        void theta_rhs_hwmod(uint); ///< Modified Hasegawa-Wakatani model

        void omega_rhs_ns(uint); ///< Navier Stokes
        void omega_rhs_lin(uint); ///< Linearized interchange model
        void omega_rhs_hw(uint); ///< Hasegawa-Wakatani model
        void omega_rhs_hwmod(uint); ///< Modified Hasegawa-Wakatani model
        void omega_rhs_hwzf(uint); /// Modified Hasegawa-Wakatani, supress zonal flows
        void omega_rhs_null(uint); ///< Zero explicit term
        void omega_rhs_ic(uint); ///< Interchange turbulence
};

#endif //SLAB_CUDA_H
// End of file slab_cuda.h
