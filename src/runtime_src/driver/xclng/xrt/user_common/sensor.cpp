#include "sensor.h"

void writeTreeJson(std::ostream &ostr, boost::property_tree::ptree &tree)
{
    boost::property_tree::json_parser::write_json( ostr, tree );
}
