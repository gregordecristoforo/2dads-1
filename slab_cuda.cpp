#include "include/slab_cuda.h"
#include "include/initialize.h"
#include "include/error.h"

/// Standard constructor for slab_cuda

/// Initializes all real and fourier fields
/// Sets pointer to RHS functions for theta and omega
slab_cuda :: slab_cuda(slab_config my_config) :
    config(my_config),
    Nx(my_config.get_nx()),
    My(my_config.get_my()),
    tlevs(my_config.get_tlevs()),
    theta(1, Nx, My), theta_x(1, Nx, My), theta_y(1, Nx, My),
    omega(1, Nx, My), omega_x(1, Nx, My), omega_y(1, Nx, My),
    strmf(1, Nx, My), strmf_x(1, Nx, My), strmf_y(1, Nx, My),
    tmp_array(1, Nx, My), 
    theta_rhs(1, Nx, My), omega_rhs(1, Nx, My),
    theta_hat(tlevs, Nx, My / 2 + 1), theta_x_hat(1, Nx, My / 2 + 1), theta_y_hat(1, Nx, My / 2 + 1),
    omega_hat(tlevs, Nx, My / 2 + 1), omega_x_hat(1, Nx, My / 2 + 1), omega_y_hat(1, Nx, My / 2 + 1),
    strmf_hat(1, Nx, My / 2 + 1), strmf_x_hat(1, Nx, My / 2 + 1), strmf_y_hat(1, Nx, My / 2 + 1),
    tmp_array_hat(1, Nx, My / 2 + 1), 
    theta_rhs_hat(tlevs - 1, Nx, My / 2 + 1),
    omega_rhs_hat(tlevs - 1, Nx, My / 2 + 1),
    dft_is_initialized(init_dft()),
    slab_output(config.get_output(), Nx, My),
    stiff_params{config.get_deltat(), config.get_lengthx(), config.get_lengthy(), config.get_model_params(0),
        config.get_model_params(1), Nx, My / 2 + 1, tlevs},
    slab_layout{config.get_xleft(), config.get_deltax(), config.get_ylow(), config.get_deltay(), Nx, My},
    block_nx_my(theta.get_block()),
    grid_nx_my(theta.get_grid()),
    block_my21_sec1(theta_hat.get_block()),
    grid_my21_sec1(theta_hat.get_grid()),
    block_my21_sec2(dim3(cuda::cuda_blockdim_nx)),
    grid_my21_sec2(dim3((Nx + cuda::cuda_blockdim_nx - 1) / cuda::cuda_blockdim_nx)),
    grid_dx_half(dim3(Nx / 2, theta_hat.get_grid().y)),
    grid_dx_single(dim3(1, theta_hat.get_grid().y))
{
    cudaDeviceSynchronize();
    //* Setting RHS pointer for theta and omega corresponding to get_?????_rhs_type() from config */
    switch(config.get_theta_rhs_type())
    {
        case twodads::rhs_t::rhs_null:
            theta_rhs_fun = &slab_cuda::theta_rhs_null;
            break;
        case twodads::rhs_t::rhs_ns:
            theta_rhs_fun = &slab_cuda::theta_rhs_ns;
            break;
        case twodads::rhs_t::theta_rhs_lin:
            theta_rhs_fun = &slab_cuda::theta_rhs_lin;
            break;
        case twodads::rhs_t::theta_rhs_log:
            theta_rhs_fun = &slab_cuda::theta_rhs_log;
            break;
        case twodads::rhs_t::theta_rhs_hw:
            theta_rhs_fun = &slab_cuda::theta_rhs_hw;
            break;
        case twodads::rhs_t::theta_rhs_hwmod:
            theta_rhs_fun = &slab_cuda::theta_rhs_hwmod;
            break;
        case twodads::rhs_t::theta_rhs_ic:
        case twodads::rhs_t::theta_rhs_NA:
        default:
            //* Throw a name_error when the RHS for theta is not ot implemented yet*/
            string err_msg("Invalid RHS: RHS for theta not implemented yet\n");
            throw name_error(err_msg);
    }

    switch(config.get_omega_rhs_type())
    {
        case twodads::rhs_t::rhs_null:
            omega_rhs_fun = &slab_cuda::omega_rhs_null;
            break;
        case twodads::rhs_t::rhs_ns:
            omega_rhs_fun = &slab_cuda::omega_rhs_ns;
            break;
        case twodads::rhs_t::omega_rhs_ic:
            omega_rhs_fun = &slab_cuda::omega_rhs_ic;
            break;
        case twodads::rhs_t::omega_rhs_hw:
            omega_rhs_fun = &slab_cuda::omega_rhs_hw;
            break;
        case twodads::rhs_t::omega_rhs_hwmod:
            omega_rhs_fun = &slab_cuda::omega_rhs_hwmod;
            break;
        case twodads::rhs_t::omega_rhs_NA:
        default:
            //* Throw a name_error when the RHS for omega is not ot implemented yet*/
            string err_msg("Invalid RHS: RHS for omega not implemented yet\n");
            throw name_error(err_msg);
    
    }

    //* Copy coefficients alpha and beta for time integration scheme to device */
    gpuErrchk(cudaMalloc((void**) &d_ss3_alpha, sizeof(cuda::ss3_alpha_r)));
    gpuErrchk(cudaMemcpy(d_ss3_alpha, &cuda::ss3_alpha_r[0], sizeof(cuda::ss3_alpha_r), cudaMemcpyHostToDevice));

    gpuErrchk(cudaMalloc((void**) &d_ss3_beta, sizeof(cuda::ss3_beta_r)));
    gpuErrchk(cudaMemcpy(d_ss3_beta, &cuda::ss3_beta_r[0], sizeof(cuda::ss3_beta_r), cudaMemcpyHostToDevice));

    //* Initialize block and grid sizes for inv_lapl and integrate_stiff */
    uint bs_y_sec12 = min(cuda::cuda_blockdim_my, My / 2);
    uint gs_y_sec12 = My / (2 * bs_y_sec12);
    uint num_blocks_sec3 = ((Nx / 2 + 1) + (cuda::cuda_blockdim_nx - 1)) / cuda::cuda_blockdim_nx;
    uint num_blocks_sec4 = ((Nx / 2 - 1) + (cuda::cuda_blockdim_nx - 1)) / cuda::cuda_blockdim_nx;

    block_sec12 = dim3(1, bs_y_sec12);
    grid_sec1 = dim3(Nx / 2 + 1, gs_y_sec12);
    grid_sec2 = dim3(Nx / 2 - 1, gs_y_sec12);
    block_sec3 = dim3(cuda::cuda_blockdim_nx);
    block_sec4 = dim3(cuda::cuda_blockdim_nx);
    grid_sec3 = dim3(num_blocks_sec3);
    grid_sec4 = dim3(num_blocks_sec4);

#ifdef DEBUG
    cout << "block_my21_sec1 = (" << block_my21_sec1.x << ", " << block_my21_sec1.y << ")\t";
    cout << "grid_my21_sec1 = (" << grid_my21_sec1.x << ", " << grid_my21_sec1.y << ")\n";
    cout << "block_my21_sec2 = (" << block_my21_sec2.x << ", " << block_my21_sec2.y << ")\t";
    cout << "grid_my21_sec2 = (" << grid_my21_sec2.x << ", " << grid_my21_sec2.y << ")\n";
    cout << "block_sec12 = (" << block_sec12.x << ", " << block_sec12.y << ")\t";
    cout << "block_sec3 = (" << block_sec3.x << ", " << block_sec3.y << ")\t";
    cout << "block_sec4 = (" << block_sec4.x << ", " << block_sec4.y << ")\t";
    cout << "grid_sec1 = (" << grid_sec1.x << ", " << grid_sec1.y << ")\n";
    cout << "grid_sec2 = (" << grid_sec2.x << ", " << grid_sec2.y << ")\n";
    cout << "grid_sec3 = (" << grid_sec3.x << ", " << grid_sec3.y << ")\n";
    cout << "grid_sec4 = (" << grid_sec4.x << ", " << grid_sec4.y << ")\n";
#endif //DEBUG
}


slab_cuda :: ~slab_cuda()
{
    finish_dft();
}


bool slab_cuda :: init_dft()
{
    cout << "slab_cuda :: init_dft()\n";
    cufftResult err;
    err = cufftPlan2d(&plan_r2c, Nx, My, CUFFT_D2Z);
    if (err != 0)
    {
        stringstream err_str;
        err_str << "Error planning D2Z DFT: " << err << "\n";
        throw gpu_error(err_str.str());
    }

    err = cufftPlan2d(&plan_c2r, Nx, My, CUFFT_Z2D);
    if (err != 0)
    {
        stringstream err_str;
        err_str << "Error planning D2Z DFT: " << err << "\n";
        throw gpu_error(err_str.str());
    }

    return(true);
}


// Cleanup cuFFT
void slab_cuda :: finish_dft()
{
#ifdef DEBUG
    cout << "slab_cuda :: finish_dft()\n";
#endif //DEBUG
    cufftDestroy(plan_r2c);
    cufftDestroy(plan_c2r);
}

// check config consistency
void slab_cuda :: test_slab_config()
{
    config.consistency();
}

// Compute initial values
// theta, omega, strmf: initialized
// theta_hat, omega_hat: non-zero values at last time index, tlev-1
// omega_rhs_hat, theta_rhs_hat: computed, non-zero value at last time index, tlev-2
void slab_cuda :: initialize()
{
    cout << "slab_cuda :: initialize()\n";
    switch(config.get_init_function())
    {
        case twodads::init_fun_t::init_NA:
            cout << "Initializing not available\n";
            break;
        case twodads::init_fun_t::init_theta_gaussian:
            cout << "Initizlizing theta gaussian\n";
            init_gaussian(&theta, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow(), config.get_log_theta());
            dft_r2c(twodads::field_t::f_theta, twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1);
            // set omega, theta to zero
            omega.set_all(0.0);
            omega_x.set_all(0.0);
            omega_y.set_all(0.0);
            strmf.set_all(0.0);
            strmf_x.set_all(0.0);
            strmf_y.set_all(0.0);

            // Copy theta_hat from tlev-1 to zero to compute derivatives
            d_dx(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_x_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_x_hat, twodads::field_t::f_theta_x, 0);

            d_dy(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_y_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_y_hat, twodads::field_t::f_theta_y, 0);
            // Zero out f_theta_hat(0,:,:)
            // Compute RHS
            rhs_fun(config.get_tlevs() - 1);
            // Copy RHS to last time level
            move_t(twodads::field_k_t::f_theta_rhs_hat, config.get_tlevs() - 2, 0);
            move_t(twodads::field_k_t::f_omega_rhs_hat, config.get_tlevs() - 2, 0);
            cout << "Done initializing\n";
            break;

        case twodads::init_fun_t::init_both_gaussian:
            cout << "Initializing theta, omega as aguasian\n";
            init_gaussian(&omega, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow(), 0);
            dft_r2c(twodads::field_t::f_omega, twodads::field_k_t::f_omega_hat, config.get_tlevs() - 1);

            init_gaussian(&theta, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow(), 0);
            dft_r2c(twodads::field_t::f_theta, twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1);

            d_dx(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_x_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_x_hat, twodads::field_t::f_theta_x, 0);

            d_dx(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_x_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_omega_x_hat, twodads::field_t::f_omega_x, 0);

            d_dy(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_y_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_y_hat, twodads::field_t::f_theta_y, 0);

            d_dy(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_y_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_omega_y_hat, twodads::field_t::f_omega_y, 0);

            break;

        case twodads::init_fun_t::init_theta_mode:
            cout << "Initializing single mode for theta\n";
            init_mode(&theta_hat, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());
            move_t(twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1, 0);

            dft_c2r(twodads::field_k_t::f_theta_hat, twodads::field_t::f_theta, config.get_tlevs() - 1);
            d_dx(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_x_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_x_hat, twodads::field_t::f_theta_x, 0);

            d_dy(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_y_hat, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_y_hat, twodads::field_t::f_theta_y, 0);

            rhs_fun(config.get_tlevs() - 1);
            move_t(twodads::field_k_t::f_theta_rhs_hat, config.get_tlevs() - 2, 0);
            move_t(twodads::field_k_t::f_omega_rhs_hat, config.get_tlevs() - 2, 0);
            break;

        case twodads::init_fun_t::init_omega_mode:
            cout << "Initializing single mode for theta\n";
            init_mode(&omega_hat, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());
            move_t(twodads::field_k_t::f_omega_hat, config.get_tlevs() - 1, 0);

            // Compute stream function and spatial derivatives for omega, and phi
            inv_laplace(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_strmf_hat, config.get_tlevs() - 1);
            d_dx(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_x_hat, config.get_tlevs() - 1);
            d_dy(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_y_hat, config.get_tlevs() - 1);
            d_dx(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_x_hat, 0);
            d_dy(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_y_hat, 0);

            dft_c2r(twodads::field_k_t::f_omega_hat, twodads::field_t::f_omega, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_omega_x_hat, twodads::field_t::f_omega_x, 0);
            dft_c2r(twodads::field_k_t::f_omega_y_hat, twodads::field_t::f_omega_y, 0);

            dft_c2r(twodads::field_k_t::f_strmf_hat, twodads::field_t::f_strmf, 0);
            dft_c2r(twodads::field_k_t::f_strmf_x_hat, twodads::field_t::f_strmf_x, 0);
            dft_c2r(twodads::field_k_t::f_strmf_y_hat, twodads::field_t::f_strmf_y, 0);

            // Move everything to their appropriate place
            rhs_fun(config.get_tlevs() - 1);
            move_t(twodads::field_k_t::f_omega_rhs_hat, config.get_tlevs() - 2, 0);
            move_t(twodads::field_k_t::f_theta_rhs_hat, config.get_tlevs() - 2, 0);
            break;

        case twodads::init_fun_t::init_both_mode:
            cout << "Initializing single modes for theta and omega\n";
            init_mode(&theta_hat, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());
            move_t(twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1, 0);

            init_mode(&omega_hat, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());
            move_t(twodads::field_k_t::f_omega_hat, config.get_tlevs() - 1, 0);

            inv_laplace(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_strmf_hat, config.get_tlevs() - 1);

            d_dx(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_x_hat, config.get_tlevs() - 1);
            d_dx(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_x_hat, config.get_tlevs() - 1);
            d_dx(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_x_hat, 0);
            d_dy(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_y_hat, config.get_tlevs() - 1);
            d_dy(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_y_hat, config.get_tlevs() - 1);
            d_dy(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_y_hat, 0);


            dft_c2r(twodads::field_k_t::f_theta_hat, twodads::field_t::f_theta, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_theta_x_hat, twodads::field_t::f_theta_x, 0);
            dft_c2r(twodads::field_k_t::f_theta_y_hat, twodads::field_t::f_theta_y, 0);

            dft_c2r(twodads::field_k_t::f_omega_hat, twodads::field_t::f_omega, config.get_tlevs() - 1);
            dft_c2r(twodads::field_k_t::f_omega_x_hat, twodads::field_t::f_omega_x, 0);
            dft_c2r(twodads::field_k_t::f_omega_y_hat, twodads::field_t::f_omega_y, 0);

            dft_c2r(twodads::field_k_t::f_strmf_hat, twodads::field_t::f_strmf, 0);
            dft_c2r(twodads::field_k_t::f_strmf_x_hat, twodads::field_t::f_strmf_x, 0);
            dft_c2r(twodads::field_k_t::f_strmf_y_hat, twodads::field_t::f_strmf_y, 0);

            rhs_fun(config.get_tlevs() - 1);
            move_t(twodads::field_k_t::f_theta_rhs_hat, config.get_tlevs() - 2, 0);
            move_t(twodads::field_k_t::f_omega_rhs_hat, config.get_tlevs() - 2, 0);
            break;


        /*
        case twodads::init_fun_t::init_omega_random_k:
            cout << "init_omega_random_k\n";
            break;
        case twodads::init_fun_t::init_omega_random_k2:
            cout << "init_omega_random_k2\n";
            break;
        case twodads::init_fun_t::init_both_random_k:
            cout << "init_both_random_k\n";
            break;
        case twodads::init_fun_t::init_omega_exp_k:
            cout << "init_omega_expk\n";
            break;
        case twodads::init_fun_t::init_omega_dlayer:
            cout << "init_omega_dlayer\n";
            break;
        case twodads::init_fun_t::init_const_k:
            cout << "init_const_k\n";
            break;
        case twodads::init_fun_t::init_theta_const:
            cout << "init_theta_const\n";
            break;
        */
        case twodads::init_fun_t::init_simple_sine:
            cout << "init_simple_sine\n";
            init_simple_sine(&theta, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());
            // Initialize omega, strmf with const. But this is done in the
            // cuda_array constructor

            // DFT of initial condition
            dft_r2c(twodads::field_t::f_theta, twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1);
            //omega_hat.set_all(0.0);
            //strmf_hat.set_all(0.0);
            break;
        case twodads::init_fun_t::init_test:
            cout << "init_test\n";
            init_invlapl(&theta, config.get_initc(), config.get_deltax(),
                    config.get_deltay(), config.get_xleft(), config.get_ylow());

            dft_r2c(twodads::field_t::f_theta, twodads::field_k_t::f_theta_hat, config.get_tlevs() - 1);
            //omega_hat.set_all(0.0);
            //strmf_hat.set_all(0.0);
        case twodads::init_fun_t::init_file:
            cout << "init_file\n";
            break;
    }
}

/// @brief Move data from time level t_src to t_dst
/// @param fname field name 
/// @param t_dst destination time index
/// @param t_src source time index
void slab_cuda :: move_t(twodads::field_t fname, uint t_dst, uint t_src)
{
    cuda_array<cuda::real_t>* arr = get_field_by_name(fname);
    arr -> move(t_dst, t_src);
}


/// @brief Move data from time level t_src to t_dst
/// @param fname field name 
/// @param t_dst destination time index
/// @param t_src source time index
void slab_cuda :: move_t(twodads::field_k_t fname, uint t_dst, uint t_src)
{
    cuda_array<cuda::cmplx_t>* arr = get_field_by_name(fname);
    arr -> move(t_dst, t_src);
}


/// @brief Copy data from time level t_src to t_dst
/// @param fname field name 
/// @param t_dst destination time index
/// @param t_src source time index
void slab_cuda :: copy_t(twodads::field_k_t fname, uint t_dst, uint t_src)
{
    cuda_array<cuda::cmplx_t>* arr = get_field_by_name(fname);
    arr -> copy(t_dst, t_src);
}


/// @brief Set fname to a constant value at time index tlev
/// @param fname field name 
/// @param val constant complex number
/// @param t_src time index
void slab_cuda::set_t(twodads::field_k_t fname, cuda::cmplx_t val, uint t_src)
{
    cuda_array<cuda::cmplx_t>* arr = get_field_by_name(fname);
    arr -> set_t(val, t_src);
}


/// @brief advance all fields with multiple time levels
void slab_cuda :: advance()
{
    theta_hat.advance();
    theta_rhs_hat.advance();
    omega_hat.advance();
    omega_rhs_hat.advance();
}


/// @brief Compute RHS from using time index t_src for dynamical fields omega_hat and theta_hat.
/// @param t_src The most current time level, only important for first transient time steps
void slab_cuda :: rhs_fun(uint t_src)
{
    (this ->* theta_rhs_fun)(t_src);
    (this ->* omega_rhs_fun)(t_src);
}


/// @brief Update real fields theta, theta_x, theta_y, etc.
/// @param tlev: The time level for theta_hat, omega_hat used as input for inverse DFT
void slab_cuda::update_real_fields(uint tlev)
{
    //Compute theta, omega, strmf and respective spatial derivatives
    d_dx(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_x_hat, tlev);
    d_dy(twodads::field_k_t::f_theta_hat, twodads::field_k_t::f_theta_y_hat, tlev);
    dft_c2r(twodads::field_k_t::f_theta_hat, twodads::field_t::f_theta, tlev);
    dft_c2r(twodads::field_k_t::f_theta_x_hat, twodads::field_t::f_theta_x, 0);
    dft_c2r(twodads::field_k_t::f_theta_y_hat, twodads::field_t::f_theta_y, 0);
    
    d_dx(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_x_hat, tlev);
    d_dy(twodads::field_k_t::f_omega_hat, twodads::field_k_t::f_omega_y_hat, tlev);
    dft_c2r(twodads::field_k_t::f_omega_hat, twodads::field_t::f_omega, tlev);
    dft_c2r(twodads::field_k_t::f_omega_x_hat, twodads::field_t::f_omega_x, 0);
    dft_c2r(twodads::field_k_t::f_omega_y_hat, twodads::field_t::f_omega_y, 0);
    
    d_dx(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_x_hat, 0);
    d_dy(twodads::field_k_t::f_strmf_hat, twodads::field_k_t::f_strmf_y_hat, 0);
    dft_c2r(twodads::field_k_t::f_strmf_hat, twodads::field_t::f_strmf, 0);
    dft_c2r(twodads::field_k_t::f_strmf_x_hat, twodads::field_t::f_strmf_x, 0);
    dft_c2r(twodads::field_k_t::f_strmf_y_hat, twodads::field_t::f_strmf_y, 0);

    dft_c2r(twodads::field_k_t::f_theta_rhs_hat, twodads::field_t::f_theta_rhs, 1);
    dft_c2r(twodads::field_k_t::f_omega_rhs_hat, twodads::field_t::f_omega_rhs, 1);
}

/// @brief execute DFT
/// @param fname_r real field type
/// @param fname_c complex field type
/// @param t time index of complex field used as target for DFT
void slab_cuda :: dft_r2c(twodads::field_t fname_r, twodads::field_k_t fname_c, uint t)
{
    cufftResult err;
    cuda_array<cuda::real_t>* arr_r = get_field_by_name(fname_r);
    cuda_array<cuda::cmplx_t>* arr_c = get_field_by_name(fname_c);
    err = cufftExecD2Z(plan_r2c, arr_r -> get_array_d(), arr_c -> get_array_d(t));
    if (err != CUFFT_SUCCESS)
        throw;
}


/// @brief execute iDFT and normalize the resulting real field
/// @param fname_c complex field type
/// @param fname_r real field type
/// @param t time index of complex field used as source for iDFT
void slab_cuda :: dft_c2r(twodads::field_k_t fname_c, twodads::field_t fname_r, uint t)
{
    cufftResult err;
    cuda_array<cuda::cmplx_t>* arr_c= get_field_by_name(fname_c);
    cuda_array<cuda::real_t>* arr_r = get_field_by_name(fname_r);
    err = cufftExecZ2D(plan_c2r, arr_c -> get_array_d(t), arr_r -> get_array_d());
    if (err != CUFFT_SUCCESS)
        throw;
    // Normalize
    arr_r -> normalize();
}


// dump real field on terminal
void slab_cuda :: dump_field(twodads::field_t field_name)
{
    cuda_array<cuda::real_t>* field = get_field_by_name(field_name);
    cout << *field << "\n";
    cudaDeviceSynchronize();
}


/// @brief dump complex field on terminal, all time levels
/// @param field_name name of complex field
void slab_cuda :: dump_field(twodads::field_k_t field_name)
{
    cuda_array<cuda::cmplx_t>* field = get_field_by_name(field_name);
    cout << *field << "\n";
}


/// @brief write full output to output.h5
/// @param time real number to be written as attribute of the datasets
void slab_cuda :: write_output(twodads::real_t time)
{
#ifdef DEBUG
    cudaDeviceSynchronize();
    cout << "Writing output, time = " << time << "\n";
#endif //DEBUG
    for(auto field_name : config.get_output())
        slab_output.surface(field_name, get_field_by_name(field_name), time);
    slab_output.output_counter++;
}


void slab_cuda ::dump_stiff_params()
{
    // Use this to test of memory is aligned between g++ and NVCC
    cout << "slab_cuda::dump_stiff_params()\n";
    cout << "config at " << (void*) &config << "\n";
    cout << "Nx at " << (void*) &Nx << "\n";
    cout << "My at " << (void*) &My << "\n";
    cout << "tlevs at " << (void*) &tlevs << "\n";
    cout << "plan_r2c at " << (void*) &plan_r2c << "\n";
    cout << "plan_c2r at " << (void*) &plan_c2r << "\n";
    cout << "slab_output at " << (void*) &slab_output << "\n";
    cout << "theta at " << (void*) &theta << "\n";
    cout << "theta_x at " << (void*) &theta_x << "\n";
    cout << "theta_y at " << (void*) &theta_y << "\n";
    cout << "slab_output at " << (void*) &slab_output << "\n";
    cout << "stiff_params at " << (void*) &stiff_params << "\n";

    cout << "slab_cuda::slab_cuda()\n";
    cout << "\nsizeof(cuda::stiff_params_t) = " << sizeof(cuda::stiff_params_t); 
    cout << "\nstiff_params at " << (void*) &stiff_params;
    cout << "\n\t.delta_t = " << stiff_params.delta_t;
    cout << "\n\t.length_x = " << stiff_params.length_x;
    cout << "\n\t.length_y = " << stiff_params.length_y;
    cout << "\n\t.diff = " << stiff_params.diff;
    cout << "\n\t.hv = " << stiff_params.hv;
    cout << "\n\t.level = " << stiff_params.level;
    cout << "\n";

    cout << "\nsizeof(cuda::slab_layout_t) = " << sizeof(cuda::slab_layout_t); 
    cout << "\nslab_layout at " << (void*) &slab_layout;
    cout << "\n\t.x_left = " << slab_layout.x_left;
    cout << "\n\t.delta_x = " << slab_layout.x_left;
    cout << "\n\t.y_lo = " << slab_layout.y_lo;
    cout << "\n\t.delta_y = " << slab_layout.delta_y;
    cout << "\n\t.Nx = " << slab_layout.Nx;
    cout << "\n\t.My = " << slab_layout.My;
    cout << "\n\n";
}



/// @brief convert field type to internal pointer
/// @param field name of the real field
cuda_array<cuda::real_t>* slab_cuda :: get_field_by_name(twodads::field_t field)
{
    switch(field)
    {
        case twodads::field_t::f_theta:
            return &theta;
            break;
        case twodads::field_t::f_theta_x:
            return &theta_x;
            break;
        case twodads::field_t::f_theta_y:
            return &theta_y;
            break;
        case twodads::field_t::f_omega:
            return &omega;
            break;
        case twodads::field_t::f_omega_x:
            return &omega_x;
            break;
        case twodads::field_t::f_omega_y:
            return &omega_y;
            break;
        case twodads::field_t::f_strmf:
            return &strmf;
            break;
        case twodads::field_t::f_strmf_x:
            return &strmf_x;
            break;
        case twodads::field_t::f_strmf_y:
            return &strmf_y;
            break;
        case twodads::field_t::f_tmp:
            return &tmp_array;
            break;
        case twodads::field_t::f_theta_rhs:
            return &theta_rhs;
            break;
        case twodads::field_t::f_omega_rhs:
            return &omega_rhs;
            break;
    }
    string err_str("Invalid field name\n");
    throw name_error(err_str);
}


/// @brief convert field type to internal pointer
/// @param field name of the complex field
cuda_array<cuda::cmplx_t>* slab_cuda :: get_field_by_name(twodads::field_k_t field)
{
    switch(field)
    {
        case twodads::field_k_t::f_theta_hat:
            return &theta_hat;
            break;
        case twodads::field_k_t::f_theta_x_hat:
            return &theta_x_hat;
            break;
        case twodads::field_k_t::f_theta_y_hat:
            return &theta_y_hat;
            break;
        case twodads::field_k_t::f_omega_hat:
            return &omega_hat;
            break;
        case twodads::field_k_t::f_omega_x_hat:
            return &omega_x_hat;
            break;
        case twodads::field_k_t::f_omega_y_hat:
            return &omega_y_hat;
            break;
        case twodads::field_k_t::f_strmf_hat:
            return &strmf_hat;
            break;
        case twodads::field_k_t::f_strmf_x_hat:
            return &strmf_x_hat;
            break;
        case twodads::field_k_t::f_strmf_y_hat:
            return &strmf_y_hat;
            break;
        case twodads::field_k_t::f_omega_rhs_hat:
            return &omega_rhs_hat;
            break;
        case twodads::field_k_t::f_theta_rhs_hat:
            return &theta_rhs_hat;
            break;
        case twodads::field_k_t::f_tmp_hat:
            return &tmp_array_hat;
            break;
    }
    string err_str("Invalid field name\n");
    throw name_error(err_str);
}


/// @brief convert field type to internal pointer
/// @param fname name of the output field
cuda_array<cuda::real_t>* slab_cuda :: get_field_by_name(twodads::output_t fname)
{
    switch(fname)
    {
        case twodads::output_t::o_theta:
            return &theta;
            break;
        case twodads::output_t::o_theta_x:
            return &theta_x;
            break;
        case twodads::output_t::o_theta_y:
            return &theta_y;
            break;
        case twodads::output_t::o_omega:
            return &omega;
            break;
        case twodads::output_t::o_omega_x:
            return &omega_x;
            break;
        case twodads::output_t::o_omega_y:
            return &omega_y;
            break;
        case twodads::output_t::o_strmf:
            return &strmf;
            break;
        case twodads::output_t::o_strmf_x:
            return &strmf_x;
            break;
        case twodads::output_t::o_strmf_y:
            return &strmf_y;
            break;
        case twodads::output_t::o_theta_rhs:
            return &theta_rhs;
            break;
        case twodads::output_t::o_omega_rhs:
            return &omega_rhs;
            break;
    }
    string err_str("get_field_by_name(twodads::output_t field): Invalid field name\n");
    throw name_error(err_str);
}


/// @brief convert field type to internal pointer
/// @param fname name of the dynamic field
cuda_array<cuda::cmplx_t>* slab_cuda :: get_field_by_name(twodads::dyn_field_t fname)
{
    switch(fname)
    {
        case twodads::dyn_field_t::d_theta:
            return &theta_hat;
            break;
        case twodads::dyn_field_t::d_omega:
            return &omega_hat;
            break;
    }
    string err_str("get_field_by_name(twodads::dyn_field_t): Invalid field name\n");
    throw name_error(err_str);
}


/// @brief convert field type to internal pointer
/// @param fname name of the RHS field
cuda_array<cuda::cmplx_t>* slab_cuda :: get_rhs_by_name(twodads::dyn_field_t fname)
{
    switch(fname)
    {
        case twodads::dyn_field_t::d_theta:
            return &theta_rhs_hat;
            break;
        case twodads::dyn_field_t::d_omega:
            return &omega_rhs_hat;
            break;
    }
    string err_str("get_rhs_by_name(twodads::dyn_field_t fname): Invalid field name\n");
    throw name_error(err_str);
}

/// @brief RHS, set explicit part for theta equation to zero
/// @param t not used 
void slab_cuda :: theta_rhs_null(uint t)
{
    theta_rhs_hat = make_cuDoubleComplex(0.0, 0.0);
}

/// @brief RHS, set explicit part for omega equation to zero
/// @param t not used
void slab_cuda :: omega_rhs_null(uint t)
{
    omega_rhs_hat = make_cuDoubleComplex(0.0, 0.0);
}

// end of file slab_cuda.cpp
