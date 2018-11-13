#include "sensor.h"

static boost::property_tree::ptree gSensorTree;

sensorTree::sensorTree()
{
}

sensorTree::~sensorTree()
{
}

void sensorTree::jsonDump(std::ostream &ostr)
{
    boost::property_tree::json_parser::write_json( ostr, tree );
}

std::string sensorTree::get(const std::string &path, std::string defaultVal)
{
    return std::string( tree.get( path, defaultVal ) );
}

boost::property_tree::ptree sensorTree::get_child(const std::string &path)
{
    return tree.get_child( path );
}

void sensorTree::add_child(const std::string &path, boost::property_tree::ptree &pt)
{
    tree.add_child( path, pt );
}
