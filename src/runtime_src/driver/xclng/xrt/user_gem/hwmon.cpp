/**
 * Copyright (C) 2017 Xilinx, Inc
 * * Author: Ryan Radjabi
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "hwmon.h"
#include "scan.h"
#include "xbsak.h"
#include <dirent.h>

/*
 * Constructor
 */
PowerMetrics::PowerMetrics(unsigned dev) : m_devIdx( dev )
{
    bool success = false;
    m_cpath = EMPTY_STRING;
    m_vpath = EMPTY_STRING;
    m_devPath = SYSFS_PATH + xcldev::pci_device_scanner::device_list[ m_devIdx ].mgmt_name;

    success = findHwmonDirs();
    if( success )
    {
        success = buildTable( m_cpath, &currentFiles, HWMON_CURR_PREFIX, HWMON_CURR_SUFFIX );
    }
    if( success )
    {
        success = findCurrents();
    }
    if( success )
    {
        success = buildTable( m_vpath, &voltageFiles, HWMON_VOLT_PREFIX, HWMON_VOLT_SUFFIX );
    }
    if( success )
    {
        success = findVoltages();
    }
    if( success )
    {
        calculateAveragePowerConsumption();
    }
}

/*
 * findHwmonDir
 */
bool PowerMetrics::findHwmonDirs()
{
    bool retVal = false;
    std::string subPath = m_devPath + "/" + HWMON_DIR + "/";
    DIR *dir = opendir( subPath.c_str() );
    struct dirent *entry = readdir( dir );
    while( entry != NULL )
    {
        if( entry->d_type == DT_DIR )
        {
            std::string s1 = entry->d_name;
            if( s1.find( HWMON_DIR ) != std::string::npos )
            {
                // read file "name" and if it is "xclmgmt_microblaze" we know it's current
                // if "xclmgmt_sysmon" it is voltage
                std::string unknown_hwmon =  subPath + entry->d_name;
                std::string hwmon_type = xcldev::get_val_string( unknown_hwmon, HWMON_TYPE_FILE );
                if( hwmon_type.find( HWMON_CURR_TYPE_NAME ) != std::string::npos )
                {
                    m_cpath = subPath + entry->d_name;
                }
                else if( hwmon_type.find( HWMON_VOLT_TYPE_NAME ) != std::string::npos )
                {
                    m_vpath = subPath + entry->d_name;
                }
                else
                {
                    // some error
                    std::cout << "Extra hwmon device found: " << hwmon_type << std::endl;
                }
            }
        }
        entry = readdir( dir );
    }
    closedir( dir );

    if( (m_cpath != EMPTY_STRING) && (m_vpath != EMPTY_STRING) )
    {
        retVal = true;
    }

    return retVal;
}

/*
 * buildTable
 */
bool PowerMetrics::buildTable( std::string path, std::vector<std::string> *list, std::string prefix, std::string suffix )
{
    bool retVal = false;
    DIR *hwmondir = opendir( path.c_str() );
    struct dirent *hwmonentry = readdir( hwmondir );
    while( hwmonentry != NULL )
    {
        if( hwmonentry->d_type == DT_REG )
        {
            std::string fname = hwmonentry->d_name;
            if( ( fname.compare( 0, prefix.size(), prefix ) == 0 ) &&
                ( fname.find( suffix ) != std::string::npos ) )
            {
                list->push_back( fname );
            }
        }
        hwmonentry = readdir( hwmondir );
    }
    closedir( hwmondir );

    sortList( list );

    if( !list->empty() )
    {
        retVal = true;
    }

    return retVal;
}

/*
 * sortList
 *
 * Assuming a list contains only entries with the same prefix such as "curr" or "in"
 * and followed by a numeral and have identical suffixes, such as "_average", this function
 * sorts them in increasing order.
 *
 * list = { "curr3_average", "curr1_average", "curr2_average" };
 * sortList( list ) => { "curr1_average", "curr2_average", "curr3_average" }
 */
void PowerMetrics::sortList(std::vector<std::string> *list)
{
    for( int j = 0; j < list->size(); j++ )
    {
        for( int i = j; i < list->size(); i++ )
        {
            if( i != j )
            {
                if( list->at( i ).compare( list->at( j ) ) < 0 )
                {
                    std::string tmp = list->at( j );
                    list->at( j ) = list->at( i );
                    list->at( i ) = tmp;
                }
            }
        }
    }
}

/*
 * findCurrents
 */
bool PowerMetrics::findCurrents()
{
    bool retVal = false;
    metrics.currents.clear();
    for( unsigned int i = 0; i < currentFiles.size(); i++ )
    {
        metrics.currents.push_back( xcldev::get_val_long( m_cpath, currentFiles.at( i ).c_str() ) );
    }
    if( !metrics.currents.empty() )
    {
        retVal = true;
    }
    return retVal;
}

/*
 * findVoltages
 *
 * Not all voltages can be read from sysmon, therefore voltages[2],[4], & [5] must be taken
 * from the definitions.
 */
bool PowerMetrics::findVoltages()
{
    bool retVal = false;
    metrics.voltages.clear();
    for( unsigned int i = 0; i < voltageFiles.size(); i++ )
    {
        metrics.voltages.push_back( xcldev::get_val_long( m_vpath, voltageFiles.at( i ).c_str() ) );
    }
    if( !metrics.voltages.empty() )
    {
        metrics.voltages.insert( metrics.voltages.begin() + HWMON_INDEX_VCC1V2, HWMON_VCC1V2_MV );
        metrics.voltages.insert( metrics.voltages.begin() + HWMON_INDEX_MGTAVCC, HWMON_MGTAVCC_MV );
        metrics.voltages.insert( metrics.voltages.begin() + HWMON_INDEX_MGTAVTT, HWMON_MGTAVTT_MV );
        retVal = true;
    }
    return retVal;
}

/*
 * getAveragePowerConsumption_mW
 */
void PowerMetrics::calculateAveragePowerConsumption()
{
    int val = 0;
    if( !currentFiles.empty() )
    {
        for( unsigned int i = 0; i < currentFiles.size(); i++ )
        {
            val += metrics.currents.at( i ) * metrics.voltages.at( i ) / MV_PER_V;
        }
    }
    metrics.totalPower_mW = val;
}

/*
 * getPowerMetrics
 */
int PowerMetrics::getTotalPower_mW()
{
    return metrics.totalPower_mW;
}
