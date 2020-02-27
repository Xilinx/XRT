
#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/vp_run_summary.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info_database.h"

namespace xdp {

  VPRunSummaryWriter::VPRunSummaryWriter(const char* filename) :
    VPWriter(filename)
  {
  }

  VPRunSummaryWriter::~VPRunSummaryWriter()
  {
  }

  void VPRunSummaryWriter::switchFiles()
  {
    // Don't actually do anything
  }

  void VPRunSummaryWriter::write(bool openNewFile)
  {
    // Ignore openNewFile
    
    // There might be more than one run summary writer if multiple
    //  plugins are instantitated.  In that case, only one will
    //  be able to write.
    if (!fout) return ;

    // Collect all the files that have been created in this host execution
    //  run and dump their information in the run summary file
    std::vector<std::pair<std::string, std::string> > files = 
      (db->getStaticInfo()).getOpenedFiles() ;

    // If there are no files, don't dump anything
    if (files.empty()) return ; 

    boost::property_tree::ptree ptRunSummary ;
    {
      boost::property_tree::ptree ptSchema ;
      ptSchema.put("major", "1") ;
      ptSchema.put("minor", "0") ;
      ptSchema.put("patch", "0") ; 
      ptRunSummary.add_child("schema_version", ptSchema) ;
    }

    boost::property_tree::ptree ptFiles ;
    for (auto f : files)
    {
      boost::property_tree::ptree ptFile ;
      ptFile.put("name", f.first.c_str()) ;
      ptFile.put("type", f.second.c_str()) ;
      ptFiles.push_back(std::make_pair("", ptFile)) ;
    }
    ptRunSummary.add_child("files", ptFiles) ;

    boost::property_tree::write_json(fout, ptRunSummary, true) ;
  }

} // end namespace xdp

