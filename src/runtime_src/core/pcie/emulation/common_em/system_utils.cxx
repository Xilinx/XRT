#include "system_utils.h"

namespace systemUtil {
  
  inline void printErrorMessage(const std::string &command, int status, const std::string& LineNo )
  {
    if (!status)
      return;      

    if (!LineNo.empty()) {  
      std::cerr << "ERROR: [EMU 60-600] " << command.c_str() << " Exception Caught - Failed with the error code " << status << " at the Line Number " << LineNo << ". PLEASE CHECK YOUR PERMISSIONS " << std::endl;
    } else {
      std::cerr << "ERROR: [EMU 60-600] " << command.c_str() << " Exception Caught - Failed with the error code " << status << ". PLEASE CHECK YOUR PERMISSIONS " << std::endl;
    }
    exit(1);
  }
  
  void makeSystemCall (std::string &operand1, systemOperation operation, std::string operand2, std::string LineNo)
  {
    std::string operationStr;
    try {
      switch (operation) {
      
        case CREATE :
          {
            operationStr = "CREATE";
            if (boost::filesystem::exists(operand1) == false)
            {
              boost::filesystem::create_directories(operand1);
            }
            break;
          }
        case REMOVE :
          {
            operationStr = "REMOVE";
            if (boost::filesystem::exists(operand1) )
            {
              boost::filesystem::remove_all(operand1);
            }
            break;
          }
        case COPY   :
          {
            operationStr = "COPY";
            std::stringstream copyCommand;
            copyCommand <<"cp "<<operand1<<" "<<operand2;
            if (boost::filesystem::exists(operand1) )
            {
              int status = system(copyCommand.str().c_str());
              printErrorMessage(copyCommand.str(),status, LineNo);
            }
            break;
          }
        case APPEND   :
          {
            operationStr = "APPEND";
            std::stringstream appendCommand;
            appendCommand <<"cat "<<operand1<<">> "<<operand2;
            if (boost::filesystem::exists(operand1) )
            {
              int status = system(appendCommand.str().c_str());
              printErrorMessage(appendCommand.str(),status, LineNo);
            }
            break;
          }
        case UNZIP  :
          {
            operationStr = "UNZIP";
            std::stringstream unzipCommand;
            unzipCommand <<"unzip -q " << operand1 << " -d " << operand2;
            int status = system(unzipCommand.str().c_str());
            printErrorMessage(unzipCommand.str(),status, LineNo);
            break;
          }
        case PERMISSIONS : 
          {
            operationStr = "PERMISSIONS";
            std::stringstream permissionsCommand ;
            permissionsCommand << "chmod -R " << operand2 << " " << operand1;
            int status = system(permissionsCommand.str().c_str());
            printErrorMessage(permissionsCommand.str(),status, LineNo);
            break;
          }
      }
    } catch (...) {
      if (!LineNo.empty()) {
        std::cerr << "ERROR: [EMU 60-601] " << " Exception Caught - Failed with the command " << operationStr.c_str() << " operation at the Line Number: " << LineNo.c_str() << std::endl;
      }
      else {
        std::cerr << "ERROR: [EMU 60-601] " << " Exception Caught - Failed with the command " << operationStr.c_str() << std::endl;
      }
      exit(1);
    }
  }
}
