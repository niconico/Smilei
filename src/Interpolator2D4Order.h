#ifndef INTERPOLATOR2D4ORDER_H
#define INTERPOLATOR2D4ORDER_H


#include "Interpolator2D.h"


//  --------------------------------------------------------------------------------------------------------------------
//! Class for 2nd order interpolator for 1d3v simulations
//  --------------------------------------------------------------------------------------------------------------------
class Interpolator2D4Order : public Interpolator2D
{
    
public:
	Interpolator2D4Order(PicParams*, SmileiMPI*);
	~Interpolator2D4Order();

	void operator() (ElectroMagn* champs, Particle* part, LocalFields* ELoc, LocalFields* BLoc);
    
private:
    double dble_1_ov_384 ;
    double dble_1_ov_48 ;
    double dble_1_ov_16 ;
    double dble_1_ov_12 ;
    double dble_1_ov_24 ;
    double dble_19_ov_96 ;
    double dble_11_ov_24 ;
    double dble_1_ov_4 ;
    double dble_1_ov_6 ;
    double dble_115_ov_192 ;
    double dble_5_ov_8 ;
    
};//END class

#endif