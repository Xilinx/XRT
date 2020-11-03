/**
 * Copyright (C) 2018-2020 Xilinx, Inc
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

#include "XclBinUtilMain.h"
#include "XclBinUtilities.h"

#include <string>
#include <iostream>
#include <exception>
#include <vector>

int main( int argc, char** argv )
{
  // Check to see if the user doesn't wish for "known" exceptions to be reported
  bool bQuiet = false;
  for (int index = 0; index < argc; ++index) {
    const static std::string sQuiet = "--quiet";
    if (sQuiet.compare(argv[index]) == 0) {
      bQuiet = true;
      break;
    }
  }

  // A kludge fix around changes in boost with regards to implicit values
  // and adjacent key value pairs.  In other words, depending on which version
  // of boost you are using, the syntax:
  //     --info myfile
  // may or may not be valued.  What is valued across all versions is:
  //     --info=myfile

  std::vector<std::string> implicitOptions = {"--info"};

  // A simply way of managing memory.  No need for a new or delete.
  std::vector<std::string> newOptions;    // Collection of strings (used instead of new/deletes)
  std::vector<const char *> argv_vector;  // Collection of string pointers

  // Examine the options
  for (int index = 0; index < argc; ++index) {
    bool optionProcessed = false;

    // Examine the current entry to determine if it is implicit
    for (const auto & option: implicitOptions) {
      if (option.compare(argv[index]) == 0) {

        // Look ahead one option to see if we need merge the two
        int peekIndex = index + 1;
        if ((peekIndex < argc) && (argv[peekIndex][0] != '-')) {
          std::string newOption = option + "=" + argv[peekIndex];
          newOptions.push_back(newOption);
          argv_vector.push_back(newOptions.back().c_str());
          index = peekIndex;
          optionProcessed = true;
          break;
        }
      }
    }
    if (optionProcessed)
      continue;

    argv_vector.push_back(argv[index]);
  }

  // We are now ready to parse the options
  const char** new_argv = argv_vector.data();
  int new_argc = (int) argv_vector.size();

  try {
    return main_(new_argc, new_argv );
  } catch( XclBinUtilities::XclBinUtilException &e) {
    if (bQuiet == false) {
      std::cerr << e.what() << std::endl;
    }
    return (int) e.exceptionType();
  } catch ( std::exception &e ) {
    std::string msg = e.what();
    if ( msg.empty() )
      std::cerr << "ERROR: Caught an internal exception no message information is available.\n";
    else {
      std::cerr << e.what() << std::endl;
    }
  } catch ( ... ) {
    std::cerr << "ERROR: Caught an internal exception no exception information is available.\n";
  }
  return -1;
}

