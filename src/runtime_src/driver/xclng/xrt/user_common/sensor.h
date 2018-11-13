#ifndef SENSOR_H
#define SENSOR_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <string>

class sensorTree {
    
public:
    // Singleton
    static sensorTree &Instance() 
    {
        static sensorTree theSensorTree;
        return theSensorTree;
    }

    template <typename T>
    void put(const std::string &path, T val)
    {
        tree.put( path, val );
    }
    void add_child(const std::string &path, boost::property_tree::ptree &pt);
    void jsonDump(std::ostream &ostr);
    std::string get(const std::string &path, std::string defaultVal = "N/A");
    boost::property_tree::ptree get_child(const std::string &path);
private:
    // hide access to constructor and destructor
    sensorTree();
    ~sensorTree();
    boost::property_tree::ptree tree;
};


#endif // SENSOR_H
