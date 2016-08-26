/*
 * cuda_array2.h
 *
 *  Created on: Oct 22, 2013
 *      Author: rku000
 *
 * Datatype to hold 2d CUDA arrays with three time levels
 *
 *  when PINNED_HOST_MEMORY is defined, memory for mirror copy of array in host
 *  memory is pinned, i.e. non-pageable. This increases memory transfer rates
 *  between host and device in exchange for more heavyweight memory allocation.
 *
 */


#ifndef CUDA_ARRAY3_H
#define CUDA_ARRAY3_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <complex>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cuComplex.h>
#include <cufft.h>
#include <string>
#include <stdio.h>
#include "check_bounds.h"
#include "error.h"
#include "cuda_types.h"



/// Error checking macro for cuda calls
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line)
{ 
    if (code != cudaSuccess) 
    {
        stringstream err_str;
        err_str << "GPUassert: " << cudaGetErrorString(code) << "\t file: " << file << ", line: " << line << "\n";
        throw gpu_error(err_str.str());
    }
}


// Verify last kernel launch
#define gpuStatus() { gpuVerifyLaunch(__FILE__, __LINE__); }
inline void gpuVerifyLaunch(const char* file, int line)
{
     cudaThreadSynchronize();
     cudaError_t error = cudaGetLastError();
     if(error != cudaSuccess)
     {
        stringstream err_str;
        err_str << "GPUassert: " << cudaGetErrorString(error) << "\t file: " << file << ", line: " << line << "\n";
        throw gpu_error(err_str.str());
     }
}



/// U is the data type for this array, f.ex. CuCmplx<double>
/// T is the base type of U, for example U=CuCmplx<double>, T = double
template <typename U, typename T>
class cuda_array{
    public:
        /// @brief Default constructor. Allocates t*nx*my*sizeof(U) bytes on the device
        cuda_array(uint t, uint m, uint n);
        cuda_array(const cuda_array<U, T>&);
        cuda_array(const cuda_array<U, T>*);
        ~cuda_array();

        U* get_array_host(int) const;

        // Test function
        void enumerate_array(const uint);
        void enumerate_array_t(const uint);

        // Operators
        cuda_array<U, T>& operator=(const cuda_array<U, T>&);
        cuda_array<U, T>& operator=(const U&);

        cuda_array<U, T>& operator+=(const cuda_array<U, T>&);
        cuda_array<U, T>& operator+=(const U&);
        cuda_array<U, T> operator+(const cuda_array<U, T>&);
        cuda_array<U, T> operator+(const U&);

        cuda_array<U, T>& operator-=(const cuda_array<U, T>&);
        cuda_array<U, T>& operator-=(const U&);
        cuda_array<U, T> operator-(const cuda_array<U, T>&);
        cuda_array<U, T> operator-(const U&);

        cuda_array<U, T>& operator*=(const cuda_array<U, T>&);
        cuda_array<U, T>& operator*=(const U&);
        cuda_array<U, T> operator*(const cuda_array<U, T>&);
        cuda_array<U, T> operator*(const U&);

        cuda_array<U, T>& operator/=(const cuda_array<U, T>&);
        cuda_array<U, T>& operator/=(const U&);
        cuda_array<U, T> operator/(const cuda_array<U, T>&);
        cuda_array<U, T> operator/(const U&);

        // Similar to operator=, but operates on all time levels
        cuda_array<U, T>& set_all(const U&);
        // Set array to constant value for specified time level
        cuda_array<U, T>& set_t(const U&, uint);
        // Access operator to host array
        U& operator()(uint, uint, uint);
        U operator()(uint, uint, uint) const;

        // Copy device memory to host and print to stdout
        friend std::ostream& operator<<(std::ostream& os, cuda_array<U, T> src)
        {
            const uint tl = src.get_tlevs();
            const uint nx = src.get_nx();
            const uint my = src.get_my();
            src.copy_device_to_host();
            os << "\n";
            for(uint t = 0; t < tl; t++)
            {
                for(uint m = 0; m < my; m++)
                {
                    for(uint n = 0; n < nx; n++)
                    {
                        // Remember to also set precision routines in CuCmplx :: operator<<
                        //os << std::setw(8) << std::setprecision(4) << src(t, m, n) << "\t";
                        os << fixed << setw(4) << src(t, m, n) << "\t";
                    }
                os << "\n";
                }
                os << "\n";
            }
            return (os);
        }

        // Copy device data into internal host buffer
        void copy_device_to_host();
        void copy_device_to_host(uint);

        // Copy device data to another buffer
        void copy_device_to_host(U*);

        // Transfer from host to device
        void copy_host_to_device();
        void copy_host_to_device(uint);

        // Advance time levels
        void advance(); 
        
        void copy(uint, uint);
        void copy(uint, const cuda_array<U, T>&, uint);
        void move(uint, uint);
        void swap(uint, uint);
        void normalize();

        void kill_kx0();
        void kill_ky0();
        void kill_k0();

        // Access to private members
        uint get_nx() const {return Nx;};
        uint get_my() const {return My;};
        inline uint get_tlevs() const {return tlevs;};
        inline int address(uint m, uint n) const {return (m * Nx + n);};
        inline dim3 get_grid() const {return grid;};
        inline dim3 get_block() const {return block;};

        // Pointer to host copy of device data
        inline U* get_array_h() const {return array_h;};
        inline U* get_array_h(uint t) const {return array_h_t[t];};

        // Pointer to device data
        inline U* get_array_d() const {return array_d;};
        inline U** get_array_d_t() const {return array_d_t;};
        inline U* get_array_d(uint t) const {return array_d_t_host[t];};

    protected: 
        // Size of data array. Host data
        const uint tlevs;
        const uint Nx;
        const uint My;

        check_bounds bounds;

        // grid and block dimension
        dim3 block;
        dim3 grid;
        // Grid for accessing all tlevs
        dim3 grid_full;
        // Array data is on device
        // Pointer to device data
        U* array_d;
        // Pointer to each time stage. Pointer to array of pointers on device
        U** array_d_t;
        // Pointer to each time stage: Pointer to each time level on host
        U** array_d_t_host;

        // Storage copy of device data on host
        U* array_h;
        U** array_h_t;
};

#ifdef __CUDACC__

__device__ inline int d_get_col() {
    return (blockIdx.x * blockDim.x + threadIdx.x);    
}


__device__ inline int d_get_row() {
    return (blockIdx.y * blockDim.y + threadIdx.y);
}


// Template kernel for d_enumerate_d and d_enumerate_c using the ca_val class
template <typename U>
__global__ void d_enumerate(U* array, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int index = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;

    printf("index = %d\nblockIdx.x = %d, blockDim.x = %d, threadIdx.x = %d, col = %d\n blockIdx.y = %d, blockDim.y = %d, threadIdx.y = %d, row = %d\n\n", index, blockIdx.x, blockDim.x, threadIdx.x, col, blockIdx.y, blockDim.y, threadIdx.y, row);
    array[index] = U(index);
}


// Template version of d_enumerate_t_x
template <typename U>
__global__ void d_enumerate_t(U** array_t, uint t, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int index = row * Nx + col;

	//if (blockIdx.x + threadIdx.x + threadIdx.y == 0)
	//	printf("blockIdx.x = %d: enumerating at t = %d, at %p(device)\n", blockIdx.x, t, array_t[t]);
    if ((col >= Nx) || (row >= My))
        return;

    array_t[t][index] = U(index);
}

template <typename U, typename T>
__global__ void d_set_constant_t(U** array_t, T val, uint t, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
	const int index = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;

    array_t[t][index] = val;
}


template <typename U>
__global__ void d_alloc_array_d_t(U** array_d_t, U* array,  uint tlevs,  uint My,  uint Nx)
{
    if (threadIdx.x >= tlevs)
        return;
    array_d_t[threadIdx.x] = &array[threadIdx.x * My * Nx];
    //printf("Device: array_d_t[%d] at %p\n", threadIdx.x, array_d_t[threadIdx.x]);
}


template <typename U>
__global__ void d_advance(U** array_t, int tlevs)
//__global__ void d_advance(U** array_t, int tlevs)
{
	U* tmp = array_t[tlevs - 1];
	int t = 0;

//#ifdef DEBUG 
//	printf("__global__ d_advance: Before:\n");
//	for(t = tlevs - 1; t >= 0; t--)
//		printf("array_t[%d] at %p\n", t, array_t[t]);
//#endif
	for(t = tlevs - 1; t > 0; t--)
		array_t[t] = array_t[t - 1];
	array_t[0] = tmp;
//#ifdef DEBUG
//	printf("__global__ d_advance: After:\n");
//	for(t = tlevs - 1; t >= 0; t--)
//		printf("array_t[%d] at %p\n", t, array_t[t]);
//#endif
}


template <typename U>
__global__ void d_swap(U**array_t, uint t1, uint t2)
{
    U* tmp = array_t[t1];
    array_t[t1] = array_t[t2];
    array_t[t2] = tmp;
}



template <typename U>
__global__ void test_alloc(U** array_t,  uint tlevs)
{
    for(int t = 0; t < tlevs; t++)
        printf("array_x_t[%d] at %p\n", t, array_t[t]);
}


// Add two arrays: complex data, specify time levels for RHS
template <typename U>
__global__ 
void d_add_arr_t(U** lhs, U** rhs, uint tlev, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    // printf("d_add_arr_t: blockIdx.x = %d, blockDim.x = %d, threadIdx.x = %d, col = %d\n", blockIdx.x, blockDim.x, threadIdx.x, col);
    if ((col >= Nx) || (row >= My))
        return;
    lhs[0][idx] += rhs[tlev][idx];
}


// Add scalar
template <typename U>
__global__ 
void d_add_scalar(U** lhs, U rhs, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;

    lhs[0][idx] += rhs;
}


// Subtract two arrays 
template <typename U>
__global__ 
void d_sub_arr_t(U** lhs, U** rhs, uint tlev, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;

    lhs[0][idx] -= rhs[tlev][idx];
}


//Subtract scalar
template <typename U>
__global__ 
void d_sub_scalar(U** lhs, U rhs, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;
   
    lhs[0][idx] -= rhs;
}


// Multiply by array: real data, specify time level for RHS
template <typename U>
__global__ 
void d_mul_arr_t(U** lhs, U** rhs, uint tlev, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;
   
    lhs[0][idx] *= rhs[tlev][idx];
}


// Multiply by scalar
template <typename U>
__global__ 
void d_mul_scalar(U** lhs, U rhs, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;
   
    lhs[0][idx] *= rhs;
}


// Divide by array: real data, specify time level for RHS
template <typename U>
__global__
void d_div_arr_t(U** lhs, U** rhs, uint tlev, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;
   
    lhs[0][idx] /= rhs[tlev][idx];
}


// Divide by scalar
template <typename U>
__global__
void d_div_scalar(U** lhs, U rhs, uint My, uint Nx)
{
    const int col = d_get_col();
    const int row = d_get_row();
    const int idx = row * Nx + col;

    if ((col >= Nx) || (row >= My))
        return;
   
    lhs[0][idx] /= rhs;
}


// Kill kx = 0 mode
template <typename U>
__global__
void d_kill_kx0(U* arr, uint My, uint Nx)
{
    const int row = d_get_row();
    const int idx = row * Nx;

    if (row > My)
        return;
    
    U zero(0.0);
    arr[idx] = zero;
}


// Kill ky=0 mode
template <typename U>
__global__
void d_kill_ky0(U* arr, uint My, uint Nx)
{
    const int col = d_get_col();
    const int idx = col;
    if (col > Nx)
        return;
    
    U zero(0.0);
    arr[idx] = zero;
}


// Kill k=0 mode
// This should be only invoked where U = cuda::cmplx_t*
template <typename U>
__global__
void d_kill_k0(U* arr)
{
    U zero(0.0);
    arr[0] = zero;
}

// Default constructor
// Template parameters: U is either CuCmplx<T> or T
// T is usually double. But why not char or short int? :D
template <typename U, typename T>
cuda_array<U, T> :: cuda_array(uint t, uint my, uint nx) :
    tlevs(t), My(my), Nx(nx), bounds(tlevs, My, Nx),
    array_d(NULL), array_d_t(NULL), array_d_t_host(NULL),
    array_h(NULL), array_h_t(NULL)
{
    // Determine grid size for kernel launch
    block = dim3(cuda::blockdim_nx, cuda::blockdim_my);
    // Round integer division for grid.y, see: http://stackoverflow.com/questions/2422712/c-rounding-integer-division-instead-of-truncating
    // a la int a = (59 + (4 - 1)) / 4;
    grid = dim3((Nx + (cuda::blockdim_nx - 1)) / cuda::blockdim_nx, My);

    // Allocate device memory
    size_t nelem = tlevs * Nx * My;
    gpuErrchk(cudaMalloc( (void**) &array_d, nelem * sizeof(U)));
    gpuErrchk(cudaMalloc( (void***) &array_d_t, tlevs * sizeof(U*)));

#ifdef PINNED_HOST_MEMORY
    // Allocate pinned host memory for faster memory transfers
    gpuErrchk(cudaMallocHost( (void**) &array_h, nelem * sizeof(U)));
    // Initialize host memory all zero
    for(int m = 0; m < My; m++)
        for(int n = 0; n < Nx; n++)
            array_h[m * Nx + n] = 0.0;
#endif
#ifndef PINNED_HOST_MEMORY
    array_h = (U*) calloc(nelem, sizeof(U));
#endif

    // array_[hd]_t is an array of pointers allocated on the host/device respectively
    array_h_t = (U**) calloc(tlevs, sizeof(U*));
    array_d_t_host = (U**) calloc(tlevs, sizeof(U*));
    // array_t[i] points to the i-th time level
    // Set pointers on device
    d_alloc_array_d_t<<<1, tlevs>>>(array_d_t, array_d, tlevs, My, Nx);
    // Update host copy
    gpuErrchk(cudaMemcpy(array_d_t_host, array_d_t, sizeof(U*) * tlevs, cudaMemcpyDeviceToHost));

    for(uint tl = 0; tl < tlevs; tl++)
        array_h_t[tl] = &array_h[tl * My * Nx];
    set_all(0.0);
//#ifdef DEBUG
//    cout << "Array size: My=" << My << ", Nx=" << Nx << ", tlevs=" << tlevs << "\n";
//    cout << "cuda::blockdim_x = " << cuda::blockdim_nx;
//    cout << ", cuda::blockdim_y = " << cuda::blockdim_my << "\n";
//    cout << "blockDim=(" << block.x << ", " << block.y << ")\n";
//    cout << "gridDim=(" << grid.x << ", " << grid.y << ")\n";
//    cout << "Device data at " << array_d << "\t";
//    cout << "Host data at " << array_h << "\t";
//    cout << nelem << " bytes of data\n";
//    for (uint tl = 0; tl < tlevs; tl++)
//    {
//        cout << "time level " << tl << " at ";
//        cout << array_h_t[tl] << "(host)\t";
//        cout << array_d_t_host[tl] << "(device)\n";
//    }
//    cout << "Testing allocation of array_d_t:\n";
//    test_alloc<<<1, 1>>>(array_d_t, tlevs);
//#endif // DEBUG
}


// copy constructor
// OBS: Cuda does not support delegating constructors yet. So this is just copy
// and paste of the standard constructor plus a memcpy of the array data
template <typename U, typename T>
cuda_array<U, T> :: cuda_array(const cuda_array<U, T>& rhs) :
    tlevs(rhs.tlevs), My(rhs.My), Nx(rhs.Nx), bounds(tlevs, My, Nx),
    block(rhs.block), grid(rhs.grid), grid_full(rhs.grid_full),
    array_d(NULL), array_d_t(NULL), array_d_t_host(NULL),
    array_h(NULL), array_h_t(NULL)
{
    uint tl = 0;
    // Allocate device memory
    size_t nelem = tlevs * My * Nx;
    gpuErrchk(cudaMalloc( (void**) &array_d, nelem * sizeof(U)));
    // Allocate pinned host memory
#ifdef PINNED_HOST_MEMORY
    gpuErrchk(cudaMallocHost( (void**) &array_h, nelem * sizeof(U)));
    // Initialize host memory all zero
    for(int m = 0; m < My; m++)
        for(int n = 0; n < Nx; n++)
            array_h[m * Nx + n] = 0.0;
#endif
#ifndef PINNED_HOST_MEMORY
    array_h = (U*) calloc(nelem, sizeof(U));
#endif

    gpuErrchk(cudaMalloc( (void***) &array_d_t, tlevs * sizeof(U*)));
    array_h_t = (U**) calloc(tlevs, sizeof(U*));
    array_d_t_host = (U**) calloc(tlevs, sizeof(U*));
    // Set pointers on device
    d_alloc_array_d_t<<<1, tlevs>>>(array_d_t, array_d, tlevs, My, Nx);
    // Update host copy
    gpuErrchk(cudaMemcpy(array_d_t_host, array_d_t, sizeof(U*) * tlevs, cudaMemcpyDeviceToHost));

    // Set time levels on host and copy time slices from rhs array to
    // local array
    for(tl = 0; tl < tlevs; tl++)
    {   
        gpuErrchk(cudaMemcpy(array_d_t_host[tl], rhs.get_array_d(tl), sizeof(U) * My * Nx, cudaMemcpyDeviceToDevice));
        array_h_t[tl] = &array_h[tl * My * Nx];
    }
    //cudaDeviceSynchronize();
}


// copy constructor
// OBS: Cuda does not support delegating constructors yet. So this is just copy
// and paste of the standard constructor plus a memcpy of the array data
template <typename U, typename T>
cuda_array<U, T> :: cuda_array(const cuda_array<U, T>* rhs) :
    tlevs(rhs -> tlevs), My(rhs -> My), Nx(rhs -> Nx), bounds(tlevs, My, Nx),
    block(rhs -> block), grid(rhs -> grid), grid_full(rhs -> grid_full),
    array_d(NULL), array_d_t(NULL), array_d_t_host(NULL),
    array_h(NULL), array_h_t(NULL)
{
    uint tl = 0;
    // Allocate device memory
    size_t nelem = tlevs * My * Nx;
    gpuErrchk(cudaMalloc( (void**) &array_d, nelem * sizeof(U)));
#ifdef PINNED_HOST_MEMORY
    // Allocate host memory
    gpuErrchk(cudaMallocHost( (void**) &array_h, nelem * sizeof(U)));
    // Initialize host memory all zero
    for(int m = 0; m < My; m++)
        for(int n = 0; n < Nx; n++)
            array_h[m * Nx + n] = 0.0;
#endif // PINNED_HOST_MEMORY
#ifndef PINNED_HOST_MEMORY
    array_h = (U*) calloc(nelem, sizeof(U));
#endif //PINNED_HOST_MEMORY
    //cerr << "cuda_array :: cuda_array() : allocated array_d_t at " << array_d << "\n";
    gpuErrchk(cudaMalloc( (void***) &array_d_t, tlevs * sizeof(U*)));
    array_h_t = (U**) calloc(tlevs, sizeof(U*));
    array_d_t_host = (U**) calloc(tlevs, sizeof(U*));
    // Set pointers on device
    d_alloc_array_d_t<<<1, tlevs>>>(array_d_t, array_d, tlevs, My, Nx);
    // Update host copy
    gpuErrchk(cudaMemcpy(array_d_t_host, array_d_t, sizeof(U*) * tlevs, cudaMemcpyDeviceToHost));

    // Set time levels on host and copy time slices from rhs array to
    // local array
    for(tl = 0; tl < tlevs; tl++)
    {   
        gpuErrchk( cudaMemcpy(array_d_t_host[tl], rhs -> get_array_d(tl), sizeof(U) * My * Nx, cudaMemcpyDeviceToDevice));
        array_h_t[tl] = &array_h[tl * My * Nx];
    }
    //cudaDeviceSynchronize();
}

template <typename U, typename T>
cuda_array<U, T> :: ~cuda_array(){
    free(array_d_t_host);
    free(array_h_t);
    cudaFree(array_d_t);
    //gpuErrchk(cudaFree(array_d_t));
#ifdef PINNED_HOST_MEMORY
    cudaFreeHost(array_h);
#endif
#ifndef PINNED_HOST_MEMORY
    free(array_h);
#endif
    //cerr << " freeing array_d at " << array_d << "\n";
    //gpuErrchk(cudaFree(&array_d));
    cudaFree(array_d);
    //cudaDeviceSynchronize();
}


// Access functions for private members

template <typename U, typename T>
void cuda_array<U, T> :: enumerate_array(const uint t)
{
	if (!bounds(t, My-1, Nx-1))
		throw out_of_bounds_err(string("cuda_array<U, T> :: enumerate_array(const int): out of bounds\n"));
	d_enumerate<<<grid, block>>>(array_d, My, Nx);
	//cudaDeviceSynchronize();
}


template <typename U, typename T>
void cuda_array<U, T> :: enumerate_array_t(const uint t)
{
	if (!bounds(t, My-1, Nx-1))
		throw out_of_bounds_err(string("cuda_array<U, T> :: enumerate_array_t(const int): out of bounds\n"));
	d_enumerate_t<<<grid, block>>>(array_d_t, t, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
}

// Operators

// Copy data fro array_d_t[0] from rhs to lhs
template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator= (const cuda_array<U, T>& rhs)
{
    // check bounds
    //if (!bounds(rhs.get_tlevs(), rhs.get_my(), rhs.get_nx()))
    if (!bounds(rhs.get_my(), rhs.get_nx()))
        throw out_of_bounds_err(string("cuda_array<U, T>& cuda_array<U, T> :: operator= (const cuda_array<U, T>& rhs): out of bounds!"));
    // Check if we assign to ourself
    if ((void*) this == (void*) &rhs)
        return *this;
    
    // Copy data from other array
    gpuErrchk(cudaMemcpy(array_d_t_host[0], rhs.get_array_d(0), sizeof(T) * My * Nx, cudaMemcpyDeviceToDevice));
    // Leave the pointer to the time leves untouched!!!
    return *this;
}


template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator= (const U& rhs)
{
    d_set_constant_t<<<grid, block>>>(array_d_t, rhs, 0, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}


// Set entire array to rhs
template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: set_all(const U& rhs)
{
	for(uint t = 0; t < tlevs; t++)
    {
		d_set_constant_t<<<grid, block>>>(array_d_t, rhs, t, My, Nx);
#ifdef DEBUG
        gpuStatus();
#endif
    }
	
	return *this;
}


// Set array to rhs for time level tlev
template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: set_t(const U& rhs, uint t)
{
    d_set_constant_t<<<grid, block>>>(array_d_t, rhs, t, My, Nx);
    return *this;
}


template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator+=(const cuda_array<U, T>& rhs)
{
    if(!bounds(rhs.get_my(), rhs.get_nx()))
        throw out_of_bounds_err(string("cuda_array<T>& cuda_array<T> :: operator+= (const cuda_array<T>& rhs): out of bounds!"));
    if ((void*) this == (void*) &rhs)
        throw operator_err(string("cuda_array<T>& cuda_array<T> :: operator+= (const cuda_array<T>&): RHS and LHS cannot be the same\n"));

    d_add_arr_t<<<grid, block>>>(array_d_t, rhs.get_array_d_t(), 0, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}


template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator+=(const U& rhs)
{
    d_add_scalar<<<grid, block>>>(array_d_t, rhs, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator+(const cuda_array<U, T>& rhs)
{
    cuda_array<U, T> result(this);
    result += rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator+(const U& rhs)
{
    cuda_array<U, T> result(this);
    result += rhs;
    return result;
}

template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator-=(const cuda_array<U, T>& rhs)
{
    if(!bounds(rhs.get_my(), rhs.get_nx()))
        throw out_of_bounds_err(string("cuda_array<T>& cuda_array<T> :: operator-= (const cuda_array<T>& rhs): out of bounds!"));
    if ((void*) this == (void*) &rhs)
        throw operator_err(string("cuda_array<T>& cuda_array<T> :: operator-= (const cuda_array<T>&): RHS and LHS cannot be the same\n"));
    
    d_sub_arr_t<<<grid, block>>>(array_d_t, rhs.get_array_d_t(), 0, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}


template<typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator-=(const U& rhs)
{
    d_sub_scalar<<<grid, block>>>(array_d_t, rhs, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator-(const cuda_array<U, T>& rhs)
{
    cuda_array<U, T> result(this);
    result -= rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator-(const U& rhs)
{
    cuda_array<U, T> result(this);
    result -= rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator*=(const cuda_array<U, T>& rhs)
{
    if(!bounds(rhs.get_my(), rhs.get_nx()))
        throw out_of_bounds_err(string("cuda_array<T>& cuda_array<T> :: operator*= (const cuda_array<T>& rhs): out of bounds!"));
    if ((void*) this == (void*) &rhs)
        throw operator_err(string("cuda_array<T>& cuda_array<T> :: operator*= (const cuda_array<T>&): RHS and LHS cannot be the same\n"));
    
    d_mul_arr_t<<<grid, block>>>(array_d_t, rhs.get_array_d_t(), 0, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}
      

template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator*=(const U& rhs)
{
    d_mul_scalar<<<grid, block>>>(array_d_t, rhs, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return (*this);
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator*(const cuda_array<U, T>& rhs)
{
    cuda_array<U, T> result(this);
    result *= rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator*(const U& rhs)
{
    cuda_array<U, T> result(this);
    result *= rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator/=(const cuda_array<U, T>& rhs)
{
    if(!bounds(rhs.get_my(), rhs.get_nx()))
        throw out_of_bounds_err(string("cuda_array<T>& cuda_array<T> :: operator*= (const cuda_array<T>& rhs): out of bounds!"));
    if ((void*) this == (void*) &rhs)
        throw operator_err(string("cuda_array<T>& cuda_array<T> :: operator*= (const cuda_array<T>&): RHS and LHS cannot be the same\n"));
    
    d_div_arr_t<<<grid, block>>>(array_d_t, rhs.get_array_d_t(), 0, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return *this;
}
      

template <typename U, typename T>
cuda_array<U, T>& cuda_array<U, T> :: operator/=(const U& rhs)
{
    d_div_scalar<<<grid, block>>>(array_d_t, rhs, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
    return (*this);
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator/(const cuda_array<U, T>& rhs)
{
    cuda_array<U, T> result(this);
    result /= rhs;
    return result;
}


template <typename U, typename T>
cuda_array<U, T> cuda_array<U, T> :: operator/(const U& rhs)
{
    cuda_array<U, T> result(this);
    result /= rhs;
    return result;
}


template <typename U, typename T>
U& cuda_array<U, T> :: operator()(uint t, uint m, uint n)
{
	if (!bounds(t, m, n))
		throw out_of_bounds_err(string("T& cuda_array<T> :: operator()(uint t, uint m, uint n): out of bounds\n"));
	return (*(array_h_t[t] + address(m, n)));
}


template <typename U, typename T>
U cuda_array<U, T> :: operator()(uint t, uint m, uint n) const
{
	if (!bounds(t, m, n))
		throw out_of_bounds_err(string("T cuda_array<T> :: operator()(uint t, uint m, uint n): out of bounds\n"));
	return (*(array_h_t[t] + address(m, n)));
}


template <typename U, typename T>
void cuda_array<U, T> :: advance()
{
	//Advance array_d_t pointer on device
    //cout << "advance\n";
    //cout << "before advancing\n";
    //for(int t = tlevs - 1; t >= 0; t--)
    //    cout << "array_d_t_host["<<t<<"] = " << array_d_t_host[t] << "\n";

    // Cycle pointer array on device and zero out last time level
	d_advance<<<1, 1>>>(array_d_t, tlevs);
    d_set_constant_t<<<grid, block>>>(array_d_t, 0.0, 0, My, Nx);
    // Cycle pointer array on host
    U* tmp = array_d_t_host[tlevs - 1];
    for(int t = tlevs - 1; t > 0; t--)
        array_d_t_host[t] = array_d_t_host[t - 1];
    array_d_t_host[0] = tmp;

    //cout << "after advancing\n";
    //for(int t = tlevs - 1; t >= 0; t--)
    //     cout << "array_d_t_host["<<t<<"] = " << array_d_t_host[t] << "\n";

    // Zero out last time level 
}


// The array is contiguous 
template <typename U, typename T>
void cuda_array<U, T> :: copy_device_to_host() 
{
    //const size_t line_size = Nx * My * tlevs * sizeof(T);
    //gpuErrchk(cudaMemcpy(array_h, array_d, line_size, cudaMemcpyDeviceToHost));
    const size_t line_size = My * Nx * sizeof(U);
    for(uint t = 0; t < tlevs; t++)
    {
        gpuErrchk(cudaMemcpy(&array_h[t * My * Nx], array_d_t_host[t], line_size, cudaMemcpyDeviceToHost));
    }
}


template <typename U, typename T>
void cuda_array<U, T> :: copy_device_to_host(uint tlev)
{
    const size_t line_size = My * Nx * sizeof(U);
    gpuErrchk(cudaMemcpy(array_h_t[tlev], array_d_t_host[tlev], line_size, cudaMemcpyDeviceToHost));
}


template <typename U, typename T>
void cuda_array<U, T> :: copy_device_to_host(U* buffer)
{
    const size_t line_size = My * Nx * sizeof(U);
    for(uint t = 0; t < tlevs; t++)
        gpuErrchk(cudaMemcpy(&buffer[t * My * Nx], array_d_t_host[t], line_size, cudaMemcpyDeviceToHost));
}


template <typename U, typename T>
void cuda_array<U, T> :: copy_host_to_device()
{
    const size_t line_size = My * Nx * sizeof(U);
    for(uint t = 0; t < tlevs; t++)
    {
        gpuErrchk(cudaMemcpy(array_d_t_host[t], &array_h[t * My * Nx], line_size, cudaMemcpyHostToDevice));
    }
}


template <typename U, typename T>
void cuda_array<U, T> :: copy_host_to_device(uint tlev)
{
    const size_t line_size = My * Nx * sizeof(U);
    gpuErrchk(cudaMemcpy(array_d_t_host[tlev], &array_h[tlev * My * Nx], line_size, cudaMemcpyHostToDevice));
}


template <typename U, typename T>
void cuda_array<U, T> :: copy(uint t_dst, uint t_src)
{
    const size_t line_size = My * Nx * sizeof(U);
    gpuErrchk(cudaMemcpy(array_d_t_host[t_dst], array_d_t_host[t_src], line_size, cudaMemcpyDeviceToDevice));
}


template <typename U, typename T>
void cuda_array<U, T> :: copy(uint t_dst, const cuda_array<U, T>& src, uint t_src)
{
    const size_t line_size = My * Nx * sizeof(U);
    gpuErrchk(cudaMemcpy(array_d_t_host[t_dst], src.get_array_d(t_src), line_size, cudaMemcpyDeviceToDevice));
}


template <typename U, typename T>
void cuda_array<U, T> :: move(uint t_dst, uint t_src)
{
    // Copy data 
    const size_t line_size = Nx * My * sizeof(U);
    gpuErrchk(cudaMemcpy(array_d_t_host[t_dst], array_d_t_host[t_src], line_size, cudaMemcpyDeviceToDevice));
    // Clear source
    d_set_constant_t<<<grid, block>>>(array_d_t, 0.0, t_src, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
}


template <typename U, typename T>
void cuda_array<U, T> :: swap(uint t1, uint t2)
{
    d_swap<<<1, 1>>>(array_d_t, t1, t2);
    gpuErrchk(cudaMemcpy(array_d_t_host, array_d_t, sizeof(U*) * tlevs, cudaMemcpyDeviceToHost));
	//cudaDeviceSynchronize();
}


template <typename U, typename T>
void cuda_array<U, T> :: kill_kx0()
{
    d_kill_kx0<<<grid.x, block.x>>>(array_d, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
}


/// Do this for U = CuCmplx<T>
template <typename U, typename T>
void cuda_array<U, T> :: kill_ky0()
{
    d_kill_ky0<<<grid.y, block.y>>>(array_d, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
}


/// Do this for U = CuCmplx<T>
template <typename U, typename T>
void cuda_array<U, T> :: kill_k0()
{
    d_kill_k0<<<1, 1>>>(array_d);
#ifdef DEBUG
    gpuStatus();
#endif
}

/// Do this for U = double, T = double
template <typename U, typename T>
inline void cuda_array<U, T> :: normalize()
{
    cuda::real_t norm = 1. / U(My * Nx);
    d_mul_scalar<<<grid, block>>>(array_d_t, norm, My, Nx);
#ifdef DEBUG
    gpuStatus();
#endif
}

#endif // CUDA_CC

#endif // CUDA_ARRAY3_H 