#include <fstream>
#include <iostream>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <nuflux/SplineFlux.h>

namespace nuflux{

  double SimpleSplineFlux::getMinEnergy()const{
    return 1e2;
  }

  double SimpleSplineFlux::getMaxEnergy()const{
    return 1e9;
  }

  double SimpleSplineFlux::readExtents(ParticleType type)const{
    double le=spline->lower_extent(0), ue=spline->upper_extent(0);
    double lc=spline->lower_extent(1), uc=spline->upper_extent(1);
    std::cout << "Extents for dim energy:\t\t" << pow(10,le) << "\t" << pow(10,ue) << std::endl;
    std::cout << "Extents for dim cos(zenith):\t" << lc << "\t" << uc << std::endl;
    return 0;
  }

  SimpleSplineFlux::SimpleSplineFlux(const std::string& fluxName):
    FluxFunction(fluxName){

    spline = boost::make_shared<photospline::splinetable<>>(detail::getDataPath("SplineFlux/"+fluxName+".fits"));
  }

  boost::shared_ptr<FluxFunction> SimpleSplineFlux::makeFlux(const std::string& fluxName){
    return(boost::dynamic_pointer_cast<FluxFunction>(boost::make_shared<SimpleSplineFlux>(fluxName)));
  }

  double SimpleSplineFlux::getFlux(ParticleType type, double energy, double cosZenith) const{
    if(type != NuMu && type != NuMuBar && type !=NuE && type!=NuEBar){
      if (isNeutrino(type)) {
        return 0;
      } else {
        throw std::runtime_error( name+" does not support particle type " + 
                                  boost::lexical_cast<std::string>(type));
      }
    }

    // Warn the user about coordinates outside of physics extents:
    double le=spline->lower_extent(0), ue=spline->upper_extent(0);
    double lc=spline->lower_extent(1), uc=spline->upper_extent(1);
    if (!((energy-pow(10, ue)) * (energy-pow(10, le)) <= 0 )){
      return 0;
    }
    if (!( (lc <= cosZenith) && (cosZenith <= uc) )){
      return 0;
    }

    double coords[2]={log10(energy),cosZenith};
    int centers[2];
    spline->searchcenters(coords,centers);
    double lf=spline->ndsplineeval(coords,centers,0);
    return(pow(10,lf));
  }

} //namespace nuflux

NNF_REGISTER_FLUX("BERSS_H3a_central",&nuflux::SimpleSplineFlux::makeFlux);
NNF_REGISTER_FLUX("BERSS_H3p_lower",&nuflux::SimpleSplineFlux::makeFlux);
NNF_REGISTER_FLUX("BERSS_H3p_central",&nuflux::SimpleSplineFlux::makeFlux);
NNF_REGISTER_FLUX("BERSS_H3p_upper",&nuflux::SimpleSplineFlux::makeFlux);
