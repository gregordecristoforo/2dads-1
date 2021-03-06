/*
 * output.h 
 * Defines an abstract type output_t and an implementation using HDF5
 */

#ifndef OUTPUT_H
#define OUTPUT_H

//#include <vector>
#include <map>
#include <H5Cpp.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "2dads_types.h"
#include "slab_config.h"
#include "cuda_array_bc_nogp.h"

#ifndef H5_NO_NAMESPACE
    using namespace H5;
#endif

// Abstract base class for output

class output_t {
public:
    output_t(slab_config_js);

    // Interface to write output in given output resource
    // write_output is purely virtual and will only be defined in the derived class

    virtual void surface(twodads::output_t, const cuda_array_bc_nogp<twodads::real_t, allocator_host>&, const size_t) = 0;
    virtual void surface(twodads::output_t, const cuda_array_bc_nogp<twodads::real_t, allocator_host>&, const size_t, const twodads::real_t) = 0;

    // Output counter and array dimensions
    inline size_t get_output_counter() const {return(output_counter);};
    inline void increment_output_counter() {output_counter++;};

    const twodads::slab_layout_t& get_geom() {return(geom);};
    twodads::real_t get_dtout() const {return(dtout);};

private:
    size_t output_counter;
    const twodads::real_t dtout;
    const twodads::slab_layout_t geom;
};


// Derived class, handles output to HDF5
class output_h5_t : public output_t {
public:
    /// Setup output for fields specified in configuration file of passed slab reference.
    output_h5_t(const slab_config_js&);
    /// Cleanup.
    ~output_h5_t();
    
    /// @brief Write output field from a host array 
    void surface(twodads::output_t, const cuda_array_bc_nogp<twodads::real_t, allocator_host>&, const size_t);
    void surface(twodads::output_t, const cuda_array_bc_nogp<twodads::real_t, allocator_host>&, const size_t, const twodads::real_t);
private:
    const std::string filename;
    H5File* output_file;
    
    Group* group_theta;
    Group* group_theta_x;
    Group* group_theta_y;
    Group* group_tau;
    Group* group_tau_x;
    Group* group_tau_y;
    Group* group_omega;
    Group* group_omega_x;
    Group* group_omega_y;
    Group* group_strmf;
    Group* group_strmf_x;
    Group* group_strmf_y;
    
    DataSpace* dspace_file;
    
    DataSpace dspace_theta;
    DataSpace dspace_theta_x;
    DataSpace dspace_theta_y;
    DataSpace dspace_tau;
    DataSpace dspace_tau_x;
    DataSpace dspace_tau_y;
    DataSpace dspace_omega;
    DataSpace dspace_omega_x;
    DataSpace dspace_omega_y;
    DataSpace dspace_strmf;
    DataSpace dspace_strmf_x;
    DataSpace dspace_strmf_y;
    DataSpace dspace_omega_rhs;
    DataSpace dspace_theta_rhs;

    DSetCreatPropList ds_creatplist;
    // Mapping from field types to dataspace
    std::map<twodads::output_t, DataSpace*> dspace_map;
    // Mapping from field types to dataspace names
    static const std::map<twodads::output_t, std::string> fname_map;
};

#endif //OUTPUT_H
