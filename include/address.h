/*
 * Address class:
 * 
 * Does addressing of array types and defines interpolators for boundary conditions
 */

#ifdef __CUDACC__
#define CUDA_MEMBER __host__ __device__
#else
#define CUDA_MEMBER
#endif

#include "cuda_types.h"
#include "error.h"
#include "bounds.h"


// Base class for ghost point interpolation.
// The derived classes implement ghost point interpolation via operator()
template <typename T>
class bval_interpolator
{
    protected: 
        // Do not instantiate bval_interpolators directly, but only via derived classes
        CUDA_MEMBER bval_interpolator(const T _bval) : bval(_bval) {};
    public:
        /// Interpolate the value outside the domain given last value inside and deltax
        /// uval is the value just inside the domain, deltax the discretization distance
        CUDA_MEMBER virtual ~bval_interpolator() {};
        CUDA_MEMBER virtual inline T operator()(const T uval, const T deltax) const {return(-40);};

        CUDA_MEMBER inline const T get_bval() const {return(bval);};
    private:
        // The value at the boundary
        const T bval;
};


template <typename T>
class bval_interpolator_dirichlet_left : public bval_interpolator<T>
{
    public:
        CUDA_MEMBER bval_interpolator_dirichlet_left(const T _bval) : bval_interpolator<T>(_bval) {};
        CUDA_MEMBER ~bval_interpolator_dirichlet_left() {};
        CUDA_MEMBER virtual inline T operator()(const T uval, const T deltax) const {return(2.0 * bval_interpolator<T>::get_bval() - uval);} ;
};


template <typename T>
class bval_interpolator_dirichlet_right : public bval_interpolator<T>
{
    public:
        CUDA_MEMBER bval_interpolator_dirichlet_right(const T _bval) : bval_interpolator<T>(_bval) {};
        CUDA_MEMBER ~bval_interpolator_dirichlet_right() {};
        CUDA_MEMBER virtual inline T operator()(const T uval, const T deltax) const {return(2.0 * bval_interpolator<T>::get_bval() - uval);};
};


template <typename T>
class bval_interpolator_neumann_left : public bval_interpolator<T>
{
    public:
        CUDA_MEMBER bval_interpolator_neumann_left(const T _bval) : bval_interpolator<T>(_bval) {}; 
        CUDA_MEMBER ~bval_interpolator_neumann_left() {};
        CUDA_MEMBER virtual inline T operator()(const T uval, const T deltax) const {return(uval - deltax * bval_interpolator<T>::get_bval());};
};


template <typename T>
class bval_interpolator_neumann_right : public bval_interpolator<T>
{
    public:
        CUDA_MEMBER bval_interpolator_neumann_right(const T _bval) : bval_interpolator<T>(_bval) {}; 
        CUDA_MEMBER ~bval_interpolator_neumann_right() {};
        CUDA_MEMBER virtual inline T operator()(const T uval, const T deltax) const {return(deltax * bval_interpolator<T>::get_bval() + uval);};
};


// Gives a functor object to access data of an array with known bounds and ghost points.
// Does not perform out-of-bounds checks when accessing elements via operator() or get_elem
template <typename T>
class address_t
{
    public:
        CUDA_MEMBER address_t(const cuda::slab_layout_t _sl, const cuda::bvals_t<T> _bv) : Nx(_sl.get_nx()), 
                                                                                           My(_sl.get_my()), 
                                                                                           pad_My(_sl.get_pad_y()), 
                                                                                           deltax(_sl.get_deltax()), 
                                                                                           deltay(_sl.get_deltay()), 
                                                                                           bv(_bv),
                                                                                           gp_interpolator_left{nullptr},
                                                                                           gp_interpolator_right{nullptr}
            {
                switch(bv.get_bc_left())
                {
                    case cuda::bc_t::bc_dirichlet:
                        gp_interpolator_left = new bval_interpolator_dirichlet_left<T>(bv.get_bv_left());
                        break;

                    case cuda::bc_t::bc_neumann:
                        gp_interpolator_left = new bval_interpolator_neumann_left<T>(bv.get_bv_left());
                        break;
                    // Periodic BCs in x are not implemented with FDs. set a nullptr and hope it fails somewhere down the line
                    // with an illegal memory access :)
                    case cuda::bc_t::bc_periodic:
                    default:
                        break;
                }
           
                switch(bv.get_bc_right())
                {
                    case cuda::bc_t::bc_dirichlet:
                        gp_interpolator_right = new bval_interpolator_dirichlet_right<T>(bv.get_bv_right());
                        break;

                    case cuda::bc_t::bc_neumann:
                        gp_interpolator_right = new bval_interpolator_neumann_right<T>(bv.get_bv_right());
                        break;
                    case cuda::bc_t::bc_periodic:
                    default:
                        break;
                }
            };

        CUDA_MEMBER ~address_t() 
        {
            delete gp_interpolator_left;
            delete gp_interpolator_right;
        }

        // Direct element access, no wrapping / ghost points
        CUDA_MEMBER T get_elem(const T* data, const int n, const int m) const
        {
            return(data[n * static_cast<int>(get_my() + get_pad_my()) + m]);
        }

        // Performs out of bounds checks: Wraps m index and accesses ghost points
        CUDA_MEMBER T operator()(const T* data, const int n, const int m) const
        {
            // Wrap m around My for periodic boundary conditions
            // 
            // m        m_wrapped
            // -2       My - 1
            // -1       My
            // 0        0
            // 1        1
            // ...
            // My - 1   My - 1
            // My       0
            // My + 1   1
            // My + 2   2
            
            const int m_wrapped = (m + static_cast<int>(get_my())) % static_cast<int>(get_my());
            T ret_val{0.0};

            if(n > -1 && n < static_cast<int>(get_nx()))
            {
                ret_val = data[n * static_cast<int>(get_my() + get_pad_my()) + m_wrapped];
            }
            else if(n == -1)
            {
                ret_val = interp_gp_left(this -> get_elem(data, 0, m_wrapped)); 
            }
            else if (n == static_cast<int>(get_nx()))
            {
                ret_val = interp_gp_right(this -> get_elem(data, static_cast<int>(get_nx() - 1), m_wrapped)); 
            }
            return(ret_val);
        }        

        CUDA_MEMBER inline size_t get_nx() const {return(Nx);};
        CUDA_MEMBER inline size_t get_my() const {return(My);};
        CUDA_MEMBER inline size_t get_pad_my() const {return(pad_My);};

        CUDA_MEMBER inline T get_deltax() const {return(deltax);};
        CUDA_MEMBER inline T get_deltay() const {return(deltay);};

        CUDA_MEMBER inline T interp_gp_left(const T uval) const {return((*gp_interpolator_left)(uval, get_deltax()));};
        CUDA_MEMBER inline T interp_gp_right(const T uval) const {return((*gp_interpolator_right)(uval, get_deltax()));};

    private:
        // Number of elements in x
        const size_t Nx;
        // Number of elements in y
        const size_t My;
        // Number of padding in y
        const size_t pad_My;
        // Sample discretization in x
        const T deltax;
        // Sample discretization in y
        const T deltay;
        // The boundary values and conditions of the array
        const cuda::bvals_t<T> bv;
        // Interpolator to get ghost points left n=-1
        bval_interpolator<T>* gp_interpolator_left;
        // Interpolator to get ghost points right, n=Nx
        bval_interpolator<T>* gp_interpolator_right;
};

// End of file address.h