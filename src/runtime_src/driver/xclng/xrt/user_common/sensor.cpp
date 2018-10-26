#include "sensor.h"

int createEmptyTree( boost::property_tree::ptree & root )
{
    if( !root.empty() )
        return -1;
    
    // create level 0 sections
    root.put( "board.info", "" );
    root.put( "board.physical", "" );
    root.put( "board.firewall", "" );
    root.put( "board.xclbin", "" );
    root.put( "board.memory", "" );
    root.put( "board.stream", "" );
    root.put( "board.compute_unit", "" );
    
    return 0;
}

int writeTree( boost::property_tree::ptree &root )
{
    if( root.empty() )
        return -1;
    
    boost::property_tree::write_json( "sensors.json", root );
    
    return 0;
}
