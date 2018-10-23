#include "sensor.h"

int createEmptyTree( boost::property_tree::ptree & root )
{
    if( !root.empty() )
        return -1;
    
    // create level 0 sections
    root.put( "board", "" );
    root.put( "power", "" );
    root.put( "firewall", "" );
    root.put( "xclbin", "" );
    root.put( "memory", "" );
    root.put( "stream", "" );
    
    return 0;
}

int writeTree( boost::property_tree::ptree &root )
{
    if( root.empty() )
        return -1;
    
    boost::property_tree::write_json( "sensors.json", root );
    
    return 0;
}
