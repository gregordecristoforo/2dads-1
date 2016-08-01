/*
 * cuda_array_bc.h
 *
 *  Created on: May 13, 2016
 *      Author: ralph
 *
 * Array type with variable padding in x- and y-direction.
 * Knows about boundary conditions, routines operating on this datatype are to compute
 * them on the fly
 *
 * Memory Layout
 *
 * rows: 0...My-1 ... My-1 + pad_y
 * cols: 0...Nx-1 ... Nx-1 + pad_x
 *
 *     0                        My-1 ... My - 1 + pad_y
 * Nx - 1 |--------- ... ------|    |
 *        |--------- ... ------|    |
 * ...
 *  0     |--------- ... ------|    |
 *
 * idx = n * (My + pad_y) + m
 *
 * columns (m, y-direction) are consecutive in memory
 *
 *
 * Mapping of CUDA threads on the array:
 *
 * Columns: 0..My - 1 + pad_y -> col = blockIdx.x * blockDim.x + threadIdx.x
 * Rows:    0..Nx - 1 + pad_x -> row = blockIdx.y * blockDim.y + threadIdx.y
 *
 * dimBlock = (blocksize_row, blocksize_col)
 * dimGrid = (My + pad_y) / blocksize_row, (My + pad_y) / blocksize_col
 *
 * Ghost points are to be computed on the fly, not stored in memory
 */

#ifndef cuda_array_bc_H_
#define cuda_array_bc_H_

#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <sstream>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <memory>
#include "bounds.h"
#include "error.h"
#include "cuda_types.h"
#include "allocator_device.h"


typedef unsigned int uint;
typedef double real_t;

// Error checking macro for cuda calls
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line)
{
    if (code != cudaSuccess)
    {
        std::stringstream err_str;
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
         std::stringstream err_str;
        err_str << "GPUassert: " << cudaGetErrorString(error) << "\t file: " << file << ", line: " << line << "\n";
        throw gpu_error(err_str.str());
     }
}

#ifdef __CUDACC__

/// Device function to compute column and row
__device__ inline size_t d_get_col() {
    return (blockIdx.x * blockDim.x + threadIdx.x);
}


__device__ inline size_t d_get_row() {
    return (blockIdx.y * blockDim.y + threadIdx.y);
}


/// Return true if row and column are within geom(excluding padded rows/cols)
/// Return false if row or column is outside the geometry
__device__ inline bool good_idx(size_t row, size_t col, const cuda::slab_layout_t geom)
{
    return((row < geom.Nx) && (col < geom.My));
}


template <typename T>
__global__
void d_alloc_array_d_t(T** array_d_t, T* array, const cuda::slab_layout_t geom, const size_t tlevs)
{
    for(size_t t = 0; t < tlevs; t++)
    {
        array_d_t[t] = &array[t * (geom.Nx + geom.pad_x) * (geom.My + geom.pad_y)];
    }
}


template <typename T>
__global__
void d_enumerate(T* array_d, const cuda::slab_layout_t geom) 
{
	const size_t col{d_get_col()};
    const size_t row{d_get_row()};
    const size_t index{row * (geom.My + geom.pad_y) + col};

#ifdef DEBUG
	if (good_idx(row, col, geom))
        printf("Good: blockIdx.x = %d, threadIdx.x = %d, col = %d, blockIdx.y = %d, threadIdx.y = %d, row = %d, index = %d\n", 
                blockIdx.x, threadIdx.x, col, blockIdx.y, threadIdx.y, row, index);
    else
        printf("Bad: blockIdx.x = %d, threadIdx.x = %d, col = %d, blockIdx.y = %d, threadIdx.y = %d, row = %d, index = %d\n", 
                blockIdx.x, threadIdx.x, col, blockIdx.y, threadIdx.y, row, index);
#endif

	if (good_idx(row, col, geom))
        array_d[index] = T(index);
}


template <typename T>
__global__
void d_set_to_zero(T** array_d_t, const cuda::slab_layout_t geom, const size_t tlev) 
{
	const size_t col{d_get_col()};
	const size_t row{d_get_row()};
	const size_t index{row * (geom.My + geom.pad_y) + col};
	
	//if (good_idx(row, col, geom))
    if((col < geom.My + geom.pad_y) & (row < geom.Nx + geom.pad_x))
		array_d_t[tlev][index] = T(0.0);
}
	


/// Evaluate the function lambda d_op_fun (with type given by template parameter O)
/// on the cell centers
template <typename T, typename O>
__global__
void d_evaluate(T* array_d_t, O d_op_fun, const cuda::slab_layout_t geom) 
{
    const size_t col{d_get_col()};
    const size_t row{d_get_row()};
    const size_t index{row * (geom.My + geom.pad_y) + col};

	if (good_idx(row, col, geom))
		array_d_t[index] = d_op_fun(row, col, geom);
}


/// Evaluate the function lambda d_op_fun (with type given by template parameter O)
/// on the cell centers. Same as d_evaluate, but pass the value at the cell center
/// to the lambda function 
template <typename T, typename O>
__global__
void d_evaluate_2(T* array_d_t, O d_op_fun, const cuda::slab_layout_t geom) 
{
	const size_t col{d_get_col()};
    const size_t row{d_get_row()};
    const size_t index{row * (geom.My + geom.pad_y) + col};

	if (good_idx(row, col, geom))
		array_d_t[index] = d_op_fun(array_d_t[index], row, col, geom);
	
}

#endif // __CUDACC_


template <typename allocator>
class cuda_array_bc_nogp{
public:

    using allocator_t = typename my_allocator_traits<allocator> :: allocator_type;
    using value_t = typename my_allocator_traits<allocator> :: value_type;
    using deleter_t = typename my_allocator_traits<allocator> :: deleter_type;
    using ptr_t = std::unique_ptr<value_t, deleter_t>;

	cuda_array_bc_nogp(const cuda::slab_layout_t, const cuda::bvals_t<value_t>, const size_t);
	~cuda_array_bc_nogp();

    /// Apply a function to the data
    template <typename F> void evaluate(F, const size_t);
    /// Enumerate data elements (debug function)
	void enumerate();
    /// Initialize all elements to zero. Making this private results in compile error:
    /// /home/rku000/source/2dads/include/cuda_array_bc_nogp.h(414): error: An explicit __device__ lambda cannot be defined in a member function that has private or protected access within its class ("cuda_array_bc_nogp")
    void initialize();

	inline value_t& operator() (const size_t n, const size_t m) {return(*(array_h + address(n, m)));};
	inline value_t operator() (const size_t n, const size_t m) const {return(*(array_h + address(n, m)));}

    inline value_t& operator() (const size_t t, const size_t n, const size_t m) {
        return(*(array_h_t[t] + address(n, m)));
    };
    inline value_t operator() (const size_t t, const size_t n, const size_t m) const {
        return(*(array_h_t[t] + address(n, m)));
    };

    // Copy device memory to host and print to stdout
    friend std::ostream& operator<<(std::ostream& os, cuda_array_bc_nogp& src)
    {
        const size_t tl{src.get_tlevs()};
        const size_t my{src.get_my()};
        const size_t nx{src.get_nx()};
        const size_t pad_x{src.get_geom().pad_x};
        const size_t pad_y{src.get_geom().pad_y};

        os << "\n";
        for(size_t t = 0; t < tl; t++)
        {
            for(size_t n = 0; n < nx + pad_x; n++)
            {
                for(size_t m = 0; m < my + pad_y; m++)
                {
                    // Remember to also set precision routines in CuCmplx :: operator<<
                	os << std::setw(cuda::io_w) << std::setprecision(cuda::io_p) << std::fixed << src(n, m) << "\t";
                }
                os << endl;
            }
            os << endl;
        }
        return (os);
    }
	
    void dump_full() const;
	
	void normalize(const size_t);
	
	// Copy entire device data to host
	void copy_device_to_host();
	// Copy entire device data to external data pointer in host memory
	//void copy_device_to_host(value_T*);
	// Copy device to host at specified time level
	//void copy_device_to_host(const size_t);

	// Copy deice data at specified time level to external pointer in device memory
	void copy_device_to_device(const size_t, value_t*);

	// Transfer from host to device
	void copy_host_to_device();
	//void copy_host_to_device(const size_t);
	//void copy_host_to_device(value_t*);

	// Advance time levels
	//void advance();

	///@brief Copy data from t_src to t_dst
	//void copy(const size_t t_dst, const size_t t_src);
	///@brief Copy data from src, t_src to t_dst
	//void copy(const size_t t_dst, const cuda_array<allocator>& src, const size_t t_src);
	///@brief Move data from t_src to t_dst, zero out t_src
	//void move(const size_t t_dst, const size_t t_src);
	///@brief swap data in t1, t2
	//void swap(const size_t t1, const size_t t2);
	//void normalize();

	//void kill_kx0();
	//void kill_ky0();
	//void kill_k0();

	// Access to private members
	inline size_t get_nx() const {return(Nx);};
	inline size_t get_my() const {return(My);};
	inline size_t get_tlevs() const {return(tlevs);};
    inline cuda::slab_layout_t get_geom() const {return(geom);};
    inline cuda::bvals_t<value_t> get_bvals() const {return(boundaries);};

	inline size_t address(size_t n, size_t m) const {
        size_t retval = n * (geom.My + geom.pad_y) + m; 
        return(retval);
    };
	inline dim3 get_grid() const {return grid;};
	inline dim3 get_block() const {return block;};

	//bounds get_bounds() const {return check_bounds;};
	// Pointer to host copy of device data
	inline value_t* get_array_h() const {return array_h;};
	inline value_t* get_array_h(size_t t) const {return array_h_t[t];};

	// smart pointer to device data, entire array
	inline ptr_t get_array_d() const {return array_d();};
	// Pointer to array of pointers, corresponding to time levels
	inline value_t** get_array_d_t() const {return array_d_t;};
	// Pointer to device data at time level t
	inline value_t* get_array_d(size_t t) const {return array_d_t_host[t];};

	// Check bounds
	inline void check_bounds(size_t t, size_t n, size_t m) const {array_bounds(t, n, m);};
	inline void check_bounds(size_t n, size_t m) const {array_bounds(n, m);};

	// Number of elements
	inline size_t get_nelem_per_t() const {return ((geom.Nx + geom.pad_x) * (geom.My + geom.pad_y));};

private:
	size_t tlevs;
	size_t Nx;
	size_t My;
	cuda::bvals_t<value_t> boundaries;
    cuda::slab_layout_t geom;
	bounds array_bounds;

    allocator my_alloc;

	// block and grid for access without ghost points, use these normally
	dim3 block;
	dim3 grid;
	
	// Array data is on device
	// Pointer to device data
	ptr_t array_d;
	// Pointer to each time stage. Pointer to array of pointers on device
	value_t** array_d_t;
	// Pointer to each time stage: Pointer to each time level on host
	value_t** array_d_t_host;
	value_t* array_h;
	value_t** array_h_t;	
	
	// CuFFT related stuff
	cufftHandle plan_fw;
	cufftHandle plan_bw;
};


const map<cuda::bc_t, string> bc_str_map
{
	{cuda::bc_t::bc_dirichlet, "Dirichlet"},
	{cuda::bc_t::bc_neumann, "Neumann"},
	{cuda::bc_t::bc_periodic, "Periodic"}
};

template < typename allocator>
cuda_array_bc_nogp<allocator> :: cuda_array_bc_nogp 
        (const cuda::slab_layout_t _geom, const cuda::bvals_t<value_t> bvals, size_t _tlevs) : 
		tlevs(_tlevs), 
        Nx(_geom.Nx), 
        My(_geom.My), 
        boundaries(bvals), 
        geom(_geom), 
        array_bounds(tlevs, Nx, My),
        block(dim3(cuda::blockdim_row, cuda::blockdim_col)),
		grid(dim3(((My + geom.pad_y) + cuda::blockdim_row - 1) / cuda::blockdim_row, 
                  ((Nx + geom.pad_x) + cuda::blockdim_col - 1) / cuda::blockdim_col)),
        array_d(my_alloc.alloc(tlevs * get_nelem_per_t() * sizeof(value_t))),
		array_d_t(nullptr),
		array_d_t_host(new value_t*[tlevs]),
		array_h(new value_t[tlevs * get_nelem_per_t()]),
		array_h_t(new value_t*[tlevs])
{
	gpuErrchk(cudaMalloc( (void***) &array_d_t, tlevs * sizeof(value_t*)));
	//gpuErrchk(cudaMalloc( (void**) &array_d, tlevs * get_nelem_per_t() * sizeof(value_t)));

    //cout << "cuda_array_bc<allocator> ::cuda_array_bc<allocator>\t";
    //cout << "Nx = " << Nx << ", pad_x = " << geom.pad_x << ", My = " << My << ", pad_y = " << geom.pad_y << endl;
    //cout << "block = ( " << block.x << ", " << block.y << ")" << endl;
    //cout << "grid = ( " << grid.x << ", " << grid.y << ")" << endl;
    cout << geom << endl;

	d_alloc_array_d_t<<<1, 1>>>(array_d_t, array_d.get(), geom, tlevs);
	gpuErrchk(cudaMemcpy(array_d_t_host, array_d_t, sizeof(value_t*) * tlevs, cudaMemcpyDeviceToHost));

	for(size_t t = 0; t < tlevs; t++)
    {
		d_set_to_zero<<<grid, block>>>(array_d_t, geom, t);
        array_h_t[t] = &array_h[t * get_nelem_per_t()];
        // Initialize array to zero
        //evaluate(*this, [=] __device__ (size_t n, size_t m, cuda::slab_layout_t geom) -> value_t
        //        {
        //            return(value_t(0.0));
        //        });
    }
    initialize();
}

    
template <typename allocator>
void cuda_array_bc_nogp<allocator> :: enumerate()
{
	for(size_t t = 0; t < tlevs; t++)
	{
		cout << t << endl;
		d_enumerate<<<grid, block>>>(get_array_d(t), geom);
	}
}


template <typename allocator>
void cuda_array_bc_nogp<allocator> :: initialize()
{
    for(size_t t = 0; t < tlevs; t++)
    {
        evaluate([=] __device__ (size_t n, size_t m, cuda::slab_layout_t geom) -> value_t
                                 {
                                    return(value_t(4.2));
                                 }, t);
    }
}


template <typename allocator>
template <typename F>
void cuda_array_bc_nogp<allocator> :: evaluate(F myfunc, const size_t tlev)
{
    d_evaluate<<<grid, block>>>(get_array_d(tlev), myfunc, geom);
}


template <typename allocator>
void cuda_array_bc_nogp<allocator> :: dump_full() const
{
	for(size_t t = 0; t < tlevs; t++)
	{
        cout << "dump_full: t = " << t << endl;
		for (size_t n = 0; n < geom.Nx + geom.pad_x; n++)
		{
			for(size_t m = 0; m < geom.My + geom.pad_y; m++)
			{
				cout << std::setw(8) << std::setprecision(5) << (*this)(t, n, m) << "\t";
                //cout << std::setw(7) << std::setprecision(5) << *(array_h_t[t] + n * (geom.My + geom.pad_y) + m) << "\t";
			}
			cout << endl;
		}
        cout << endl << endl;
	}
}


template <typename allocator>
void cuda_array_bc_nogp<allocator> :: normalize(const size_t tlev)
{
    // If we made a 1d DFT normalize by My. Otherwise nomalize by Nx * My
    switch (boundaries.bc_left)
    {
        case cuda::bc_t::bc_dirichlet:
            // fall through
        case cuda::bc_t::bc_neumann:
            d_evaluate_2<<<grid, block>>>(get_array_d(tlev), [=] __device__ (value_t in, size_t n, size_t m, cuda::slab_layout_t geom) -> value_t
            {
                return(in / value_t(geom.My));
            }, geom);
            break;

        case cuda::bc_t::bc_periodic:
            d_evaluate_2<<<grid, block>>>(get_array_d(tlev), [=] __device__ (value_t in, size_t n, size_t m, cuda::slab_layout_t geom) -> value_t
            {
                return(in / value_t(geom.Nx * geom.My));
            }, geom);
            break;
    }
}


template <typename allocator>
void cuda_array_bc_nogp<allocator>::copy_device_to_host()
{
    for(size_t t = 0; t < tlevs; t++)
        gpuErrchk(cudaMemcpy(&array_h[t * get_nelem_per_t()], get_array_d(t), sizeof(value_t) * get_nelem_per_t(), cudaMemcpyDeviceToHost));	
}


template <typename allocator>
void cuda_array_bc_nogp<allocator> :: copy_host_to_device()
{ 
    for(size_t t = 0; t < tlevs; t++)
        gpuErrchk(cudaMemcpy(&array_h[t * get_nelem_per_t()], get_array_d(t), sizeof(value_t) * get_nelem_per_t(), cudaMemcpyDeviceToHost));
}


template <typename allocator>
cuda_array_bc_nogp<allocator> :: ~cuda_array_bc_nogp()
{
	if(array_h != nullptr)
		delete [] array_h;
	if(array_h_t != nullptr)
		delete [] array_h_t;
	if(array_d_t != nullptr)
		cudaFree(array_d_t);
		
	cufftDestroy(plan_fw);
	cufftDestroy(plan_bw);
}

#endif /* cuda_array_bc_H_ */
