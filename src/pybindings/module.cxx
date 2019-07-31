#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/python.hpp>
#include <boost/python/to_python_converter.hpp>
#include <nuflux/nuflux.h>

namespace bp = boost::python;

#if defined(__has_include)
#if __has_include(<numpy/ndarrayobject.h>)
#define USE_NUMPY
#endif
#endif

#ifdef USE_NUMPY
#include <numpy/ndarrayobject.h>

bp::object
make_array(bp::object obj, int typenum)
{
  PyObject *ptype_arr = PyArray_FromAny(obj.ptr(), PyArray_DescrFromType(typenum), 0, 0, NPY_DEFAULT, NULL);
  if (PyErr_Occurred())
    throw bp::error_already_set();
  return bp::object(bp::detail::new_reference(ptype_arr));
}

bp::object
getFlux(const NewNuFlux::FluxFunction &self, bp::object ptype_obj, bp::object energy_obj, bp::object cos_theta_obj)
{
  // Cast inputs to ndarray. These are wrapped in boost::python::object so
  // they can be properly destroyed if exceptions are raised.
  bp::object ptype_arr = make_array(ptype_obj, NPY_INT);
  bp::object energy_arr = make_array(energy_obj, NPY_DOUBLE);
  bp::object cos_theta_arr = make_array(cos_theta_obj, NPY_DOUBLE);
  
  // Set up a broadcast iterator to get the shape of the output array
  bp::object iter = bp::object(bp::detail::new_reference(PyArray_MultiIterNew(3,
    ptype_arr.ptr(), energy_arr.ptr(), cos_theta_arr.ptr())));
  if (PyErr_Occurred())
    throw bp::error_already_set();
  // Allocate output array
  PyArrayObject *out_arr = reinterpret_cast<PyArrayObject*>(PyArray_SimpleNew(
    reinterpret_cast<PyArrayMultiIterObject*>(iter.ptr())->nd,
    reinterpret_cast<PyArrayMultiIterObject*>(iter.ptr())->dimensions, NPY_DOUBLE));
  if (PyErr_Occurred())
    throw bp::error_already_set();
  // Set up broadcast iterator (again) to loop over input and output elements
  iter = bp::object(bp::detail::new_reference(PyArray_MultiIterNew(4,
    ptype_arr.ptr(), energy_arr.ptr(), cos_theta_arr.ptr(), out_arr)));
  if (PyErr_Occurred())
    throw bp::error_already_set();
  
  while (PyArray_MultiIter_NOTDONE(iter.ptr())) {
    I3Particle::ParticleType *ptype = reinterpret_cast<I3Particle::ParticleType*>(PyArray_MultiIter_DATA(iter.ptr(), 0));
    double *energy = reinterpret_cast<double*>(PyArray_MultiIter_DATA(iter.ptr(), 1));
    double *cos_theta = reinterpret_cast<double*>(PyArray_MultiIter_DATA(iter.ptr(), 2));
    double *out = reinterpret_cast<double*>(PyArray_MultiIter_DATA(iter.ptr(), 3));
    
    *out = self.getFlux(*ptype, *energy, *cos_theta);
    PyArray_MultiIter_NEXT(iter.ptr());
  }
  
  return bp::object(bp::detail::new_reference(PyArray_Return(out_arr)));
}
#endif

bool isStandAlone=true;

void
register_FluxFunction()
{
  using namespace NewNuFlux;
  
  bp::def("makeFlux", &makeFlux, "Instantiate and return a flux model");
  bp::def("availableFluxes", &availableFluxes, "Get a list of valid flux model names");
  bp::def("kneesForFlux", &kneesForFlux, "Get a list of valid knee-reweighting scheme names for the given model");
  bp::def("printModels", &printModels, "Print a list of models to stdout");
  
  bp::class_<FluxFunction, boost::shared_ptr<FluxFunction>, boost::noncopyable>("FluxFunction", bp::no_init)
    .def("getFlux",
#ifdef USE_NUMPY
         &getFlux,
#else
         (double (FluxFunction::*)(I3Particle::ParticleType type,double,double) const)&FluxFunction::getFlux,
#endif
         "Get the flux of neutrinos predicted by this model. \n\n"
         ":param particle_type: type of neutrino\n"
         ":param energy: energy of neutrino in GeV\n"
         ":param cos_zen: cosine of the zenith angle in radians\n\n"
         ":returns: a differential flux in units of 1/GeV cm^2 sr s",
         (bp::args("particle_type"), "energy", "cos_zen"))
    .add_property("name", &FluxFunction::getName)
    ;
  
  bp::class_<KneeReweightable, boost::shared_ptr<KneeReweightable>, boost::noncopyable>("KneeReweightable", bp::no_init)
    .add_property("knee_reweighting_model", &KneeReweightable::getKneeReweightingModel, &KneeReweightable::setKneeReweightingModel)
    ;
  
  bp::class_<PionKaonAdjustable, boost::shared_ptr<PionKaonAdjustable>, boost::noncopyable>("PionKaonAdjustable", bp::no_init)
    .add_property("relative_pion_contribution", &PionKaonAdjustable::getRelativePionContribution, &PionKaonAdjustable::setRelativePionContribution)
    .add_property("relative_kaon_contribution", &PionKaonAdjustable::getRelativeKaonContribution, &PionKaonAdjustable::setRelativeKaonContribution)
    ;
  
  bp::class_<LegacyConventionalFlux, boost::shared_ptr<LegacyConventionalFlux>,
             bp::bases<FluxFunction, PionKaonAdjustable, KneeReweightable> >("LegacyConventionalFlux", bp::no_init)
    ;
  
  bp::class_<LegacyPromptFlux, boost::shared_ptr<LegacyPromptFlux>,
             bp::bases<FluxFunction, KneeReweightable> >("LegacyPromptFlux", bp::no_init)
    ;
  

  {
    using namespace I3Particle;
#define ENUM_DEF(r,data,T) .value(BOOST_PP_STRINGIZE(T), data::T)
    bp::enum_<ParticleType>("ParticleType")
      BOOST_PP_SEQ_FOR_EACH(ENUM_DEF,I3Particle,I3PARTICLE_H_I3Particle_ParticleType)
      .export_values()
      ;
#undef ENUM_DEF
  }
  
  
#ifndef NO_PHOTOSPLINE
  bp::class_<SimpleSplineFlux, boost::shared_ptr<SimpleSplineFlux>,
             bp::bases<FluxFunction>, boost::noncopyable >("SplineFlux", bp::no_init)
    ;
#endif
  
  bp::scope().attr("stand_alone") = isStandAlone;
}

namespace NewNuFlux{
  namespace detail{
    std::string getDataPath(std::string fname){
      bp::object pkg_resources = bp::import("pkg_resources");
      bp::object resource_filename = pkg_resources.attr("resource_filename");
      std::string path = bp::extract<std::string>(resource_filename("nuflux","data/"+fname));
      return path;
    }
  }
}

BOOST_PYTHON_MODULE(_nuflux)
{  
#ifdef USE_NUMPY
  import_array();
#endif  
  register_FluxFunction();
}



