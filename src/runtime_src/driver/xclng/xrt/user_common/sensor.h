#ifndef SENSOR_H
#define SENSOR_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

static boost::property_tree::ptree gSensorTree;

int createEmptyTree( boost::property_tree::ptree & root );
int writeTree( boost::property_tree::ptree &root );

#endif // SENSOR_H
