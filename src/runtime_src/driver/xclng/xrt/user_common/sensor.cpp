#include "sensor.h"

int createEmptyTree( boost::property_tree::ptree & root )
{
    if( !root.empty() )
        return -1;
    
    // create level 0 sections
//    root.put( "board.info", "" );
//    root.put( "board.physical", "" );
//    root.put( "board.firewall", "" );
//    root.put( "board.xclbin", "" );
//    root.put( "board.memory", "" );
//    root.put( "board.stream", "" );
//    root.put( "board.compute_unit", "" );
    
    return 0;
}

int dumpPropertyTree( std::ostream &ostr )
{
    ostr << std::left;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "XRT\n   Version: " << gSensorTree.get( "runtime.build.version", "N/A" )
            << "\n   Date:    " << gSensorTree.get( "runtime.build.hash_date", "N/A" )
            << "\n   Hash:    " << gSensorTree.get( "runtime.build.hash", "N/A" ) << std::endl;
    ostr << "DSA name\n" << gSensorTree.get( "board.info.dsa_name", "N/A" ) << std::endl;
    ostr << std::setw(16) << "Vendor" << std::setw(16) << "Device" << std::setw(16) << "SubDevice" << std::setw(16) << "SubVendor" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.info.vendor", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.device", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.subdevice", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.subvendor", "N/A" ) << std::endl;
    ostr << std::setw(16) << "DDR size" << std::setw(16) << "DDR count" << std::setw(16) << "OCL Frequency" << std::setw(16) << "Clock0" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.info.ddr_size", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.ddr_count", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.ocl_freq", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.clock0", "N/A" ) << std::endl;
    ostr << std::setw(16) << "PCIe" 
         << std::setw(16) << "DMA bi-directional threads" 
         << std::setw(16) << "MIG Calibrated" << std::endl;
    ostr << "GEN " << gSensorTree.get( "board.info.pcie_speed", "N/A" ) << "x" << std::setw(10) << gSensorTree.get( "board.info.pcie_width", "N/A" )
         << std::setw(32) << gSensorTree.get( "board.info.dma_threads", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.info.mig_calibrated", "N/A" ) << std::endl;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "Temperature (C):\n";
    ostr << std::setw(16) << "PCB TOP FRONT" << std::setw(16) << "PCB TOP REAR" << std::setw(16) << "PCB BTM FRONT" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.thermal.pcb.top_front", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.thermal.pcb.top_rear", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.thermal.pcb.btm_front", "N/A" ) << std::endl;
    ostr << std::setw(16) << "FPGA TEMP" << std::setw(16) << "TCRIT Temp" << std::setw(16) << "FAN Speed (RPM)" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.thermal.fpga_temp", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.thermal.tcrit_temp", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.thermal.fan_speed_rpm", "N/A" ) << std::endl;
    ostr << "Electrical (mV), (mA):\n";
    ostr << std::setw(16) << "12V PEX" << std::setw(16) << "12V AUX" << std::setw(16) << "12V PEX Current" << std::setw(16) << "12V AUX Current" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.electrical.12v_pex.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.12v_aux.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.12v_pex.current", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.12v_aux.current", "N/A" ) << std::endl;
    ostr << std::setw(16) << "3V3 PEX" << std::setw(16) << "3V3 AUX" << std::setw(16) << "DDR VPP BOTTOM" << std::setw(16) << "DDR VPP TOP" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.electrical.3v3_pex.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.3v3_aux.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.ddr_vpp_bottom.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.ddr_vpp_top.voltage", "N/A" ) << std::endl;
    ostr << std::setw(16) << "SYS 5V5" << std::setw(16) << "1V2 TOP" << std::setw(16) << "1V8 TOP" << std::setw(16) << "0V85" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.electrical.sys_v5v.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.1v2_top.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.1v8_top.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.0v85.voltage", "N/A" ) << std::endl;
    ostr << std::setw(16) << "MGT 0V9" << std::setw(16) << "12V SW" << std::setw(16) << "MGT VTT" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.electrical.mgt_0v9.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.12v_sw.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.mgt_vtt.voltage", "N/A" ) << std::endl;
    ostr << std::setw(16) << "VCCINT VOL" << std::setw(16) << "VCCINT CURR" << std::setw(16) << "DNA" << std::endl;
    ostr << std::setw(16) << gSensorTree.get( "board.physical.electrical.vccint.voltage", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.vccint.current", "N/A" )
         << std::setw(16) << gSensorTree.get( "board.physical.electrical.dna", "N/A" ) << std::endl;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "Firewall Last Error Status:\n";
    ostr << " Level " << std::setw(2) << gSensorTree.get( "board.error.firewall.firewall_level", "N/A" ) << ": 0x0"
         << gSensorTree.get( "board.error.firewall.status", "N/A" ) << std::endl;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << std::left << std::setw(48) << "Mem Topology"
         << std::setw(32) << "Device Memory Usage" << std::endl;
    ostr << std::setw(16) << "Tag"  << std::setw(12) << "Type"
         << std::setw(12) << "Temp" << std::setw(8) << "Size";
    ostr << std::setw(16) << "Mem Usage" << std::setw(8) << "BO nums" << std::endl;
    

    BOOST_FOREACH( const boost::property_tree::ptree::value_type &v, gSensorTree.get_child( "board.memory" ) ) {
        if( v.first == "mem" ) {
            int mem_index = -1;
            int mem_used = -1;
            std::string mem_tag = "N/A";
            std::string mem_size = "N/A";
            std::string mem_type = "N/A";
            std::string val;
            BOOST_FOREACH( const boost::property_tree::ptree::value_type &subv, v.second ) {
                val = subv.second.get_value<std::string>();
                if( subv.first == "index" ) 
                    mem_index = subv.second.get_value<int>();
                else if( subv.first == "type" )
                    mem_type = val;
                else if( subv.first == "tag" )
                    mem_tag = val;
                else if( subv.first == "used" )
                    mem_used = subv.second.get_value<int>();
                else if( subv.first == "size" )
                    mem_size = val;
            }
            ostr << std::left
                 << std::setw(2) << "[" << mem_index << "] "
                 << std::left << std::setw(14) << mem_tag 
                 << std::setw(12) << " " << mem_type << " " 
                 << std::setw(12) << mem_size << " " 
                 << std::setw(16) << mem_used << std::endl;
        }
    }
    ostr << "Total DMA Transfer Metrics:" << std::endl;
    BOOST_FOREACH( const boost::property_tree::ptree::value_type &v, gSensorTree.get_child( "board.pcie_dma.transfer_metrics" ) ) {
        if( v.first == "chan" ) {
            std::string chan_index, chan_h2c, chan_c2h, chan_val = "N/A";
            BOOST_FOREACH( const boost::property_tree::ptree::value_type &subv, v.second ) {
                chan_val = subv.second.get_value<std::string>();
                if( subv.first == "index" )
                    chan_index = chan_val;
                else if( subv.first == "h2c" )
                    chan_h2c = chan_val;
                else if( subv.first == "c2h" )
                    chan_c2h = chan_val;
            }
            ostr << "  Chan[" << chan_index << "].h2c:  " << chan_h2c << std::endl;
            ostr << "  Chan[" << chan_index << "].c2h:  " << chan_c2h << std::endl;
        }
    }
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "Stream Topology, TODO\n";
    ostr << "#################################\n";
    ostr << "XCLBIN ID:\n";
    ostr << gSensorTree.get( "board.xclbin.uid", "0" ) << std::endl;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "Compute Unit Status:\n";
    BOOST_FOREACH( const boost::property_tree::ptree::value_type &v, gSensorTree.get_child( "board.compute_unit" ) ) {
        if( v.first == "cu" ) {
            std::string val, cu_i, cu_n, cu_ba, cu_s = "N/A";
            BOOST_FOREACH( const boost::property_tree::ptree::value_type &subv, v.second ) {
                val = subv.second.get_value<std::string>();
                if( subv.first == "count" ) 
                    cu_i = val;
                else if( subv.first == "name" )
                    cu_n = val;
                else if( subv.first == "base_address" )
                    cu_ba = val;
                else if( subv.first == "status" )
                    cu_s = val;
            }
            ostr << std::setw(6) << "CU[" << cu_i << "]: "
                 << std::setw(16) << cu_n 
                 << std::setw(7) << "@0x" << std::hex << cu_ba << " " 
                 << std::setw(10) << cu_s << std::endl;
        }
    }
    
    return 0; //TODO
}

/*
 * wrapper around write_json
 */
void writeJsonFile( std::string filename )
{
    boost::property_tree::write_json( std::string( filename + ".json" ), gSensorTree );
}
