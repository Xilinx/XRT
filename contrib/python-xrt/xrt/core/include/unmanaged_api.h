#ifndef __UNMANAGED_API__
#define __UNMANAGED_API__

#include <iostream>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include "xclhal2.h"
#include "ert.h"

using namespace std;
namespace py = boost::python;
namespace np = boost::python::numpy;

py::list unmanaged_read(string device_name, unsigned flags, unsigned long size, unsigned long offset);

void unmanaged_write(string device_name, unsigned flags, unsigned long size, unsigned long offset, py::list data);

#endif