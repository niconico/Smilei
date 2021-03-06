
#ifndef ELECTROMAGNBC2D_Trans_DAMPING_H
#define ELECTROMAGNBC2D_Trans_DAMPING_H

#include "ElectroMagnBC.h" 

class Params;
class ElectroMagn;

class ElectroMagnBC2D_Trans_Damping : public ElectroMagnBC {
public:
    ElectroMagnBC2D_Trans_Damping( Params &params, Patch* patch );
    ~ElectroMagnBC2D_Trans_Damping();
    
    virtual void apply(ElectroMagn* EMfields, double time_dual, Patch* patch);
    
private:
    //! Number of nodes on the primal grid in the x-direction
    unsigned int nx_p;
    
    //! Number of nodes on the dual grid in the x-direction
    unsigned int nx_d;
    
    //! Number of nodes on the primal grid in the y-direction
    unsigned int ny_p;
    
    //! Number of nodes on the dual grid in the y-direction
    unsigned int ny_d;
    
    
    // number of dumping layers
    unsigned int ny_l;
    // Damping coefficient
    double cdamp; 
    // array of coefficient per layer
    std::vector<double> coeff;
    
    
};

#endif

