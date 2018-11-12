#ifndef SENSOR_H
#define SENSOR_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

static boost::property_tree::ptree gSensorTree;

void writeTreeJson( std::ostream &ostr, boost::property_tree::ptree &tree );

#endif // SENSOR_H
