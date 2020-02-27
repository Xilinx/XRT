
#ifndef VP_RUN_SUMMARY_DOT_H
#define VP_RUN_SUMMARY_DOT_H

#include <vector>
#include <string>

#include "xdp/profile/writer/vp_base/vp_writer.h"
#include "xdp/config.h"

namespace xdp {

  class VPRunSummaryWriter : public VPWriter
  {
  private:
  protected:
    virtual void switchFiles() ;
  public:
    VPRunSummaryWriter(const char* filename) ;
    ~VPRunSummaryWriter() ;

    virtual void write(bool openNewFile) ;
  } ;

} // end namespace xdp

#endif
