#include "system_utils.h"

namespace systemUtil {
  
  inline void printErrorMessage(const std::string &command, int status)
  {
    if (!status)
      return;      
      
    std::cerr << "ERROR: [EMU 60-600] " << command.c_str() << " Exception Caught - Failed with the error code " << status << ". PLEASE CHECK YOUR PERMISSIONS " << std::endl;
    exit(0);
  }
  
  void makeSystemCall (std::string &operand1, systemOperation operation, std::string operand2 )
  {
    switch (operation) {

      case CREATE :
        {
          if (boost::filesystem::exists(operand1) == false)
          {
            boost::filesystem::create_directories(operand1);
          }
          break;
        }
      case REMOVE :
        {
          if (boost::filesystem::exists(operand1) )
          {
            boost::filesystem::remove_all(operand1);
          }
          break;
        }
      case COPY   :
        {
          std::stringstream copyCommand;
          copyCommand <<"cp "<<operand1<<" "<<operand2;
          if (boost::filesystem::exists(operand1) )
          {
            int status = system(copyCommand.str().c_str());
            printErrorMessage(copyCommand.str(),status);
          }
          break;
        }
      case APPEND   :
        {
          std::stringstream appendCommand;
          appendCommand <<"cat "<<operand1<<">> "<<operand2;
          if (boost::filesystem::exists(operand1) )
          {
            int status = system(appendCommand.str().c_str());
            printErrorMessage(appendCommand.str(),status);
          }
          break;
        }
      case UNZIP  :
        {
          std::stringstream unzipCommand;
          unzipCommand <<"unzip -q " << operand1 << " -d " << operand2;
          int status = system(unzipCommand.str().c_str());
          printErrorMessage(unzipCommand.str(),status);
          break;
        }
      case PERMISSIONS : 
        {
          std::stringstream permissionsCommand ;
          permissionsCommand << "chmod -R " << operand2 << " " << operand1;
          int status = system(permissionsCommand.str().c_str());
          printErrorMessage(permissionsCommand.str(),status);
          break;
        }
    }
  }
}
