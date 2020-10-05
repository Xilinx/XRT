/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_PROFILE_H_
#define _XRT_PROFILE_H_

#include "xrt.h"

#ifdef __cplusplus

namespace xrt { namespace profile {

  class user_range
  {
  private:
    uint32_t id ;
    bool active ;
  public:
    /**
     * user_range() - Default Constructor for user range
     *
     * If this constructor is called, the object is created but the 
     * beginning of the range time is not marked.
     */
    XCL_DRIVER_DLLESPEC
    user_range() ;

    /**
     * user_range() - Constructor for user range with text
     *
     * @label:   The string that appears embedded on the waveform for this range
     * @tooltip: The string that appears on the waveform when hovering 
     *           over the range
     *
     * Construct an object and start keeping track of time immediately
     * upon construction.
     */
    XCL_DRIVER_DLLESPEC
    user_range(const char* label, const char* tooltip) ;

    /**
     * user_range() - Copy constructor
     * 
     * Explicitly removed as ranges are actively keeping track of real time
     *  and cannot be copied.
     */
    user_range(const user_range& rhs) = delete ;

    /**
     * user_range() - Move constructor
     *
     * Explicitly removed as ranges are actively keeping track of real time
     *  and cannot be moved.
     */
    user_range(user_range&& rhs) = delete ;

    /**
     * operator=()
     * 
     * Explicitly removed as ranges should not be assigned
     */
    user_range& operator=(const user_range& rhs) = delete ;
    user_range& operator=(user_range&& rhs) = delete ;

    /**
     * ~user_range() - Destructor
     *
     * If the range is still active, mark this point as the end of the range
     */
    XCL_DRIVER_DLLESPEC
    ~user_range() ;

    /**
     * start() - Mark the start position of a user range
     *
     * If the range is still actively recording time, end the current
     * range and start a new one.
     */
    XCL_DRIVER_DLLESPEC
    void start(const char* label, const char* tooltip) ;

    /**
     * end() - Mark the end position of a user range
     *
     * If the range has already been ended, do nothing.  Otherwise
     * mark the end position of the range and stop tracking time.
     */    
    XCL_DRIVER_DLLESPEC
    void end() ;
  } ;

  class user_event
  {
  public:
    /**
     * user_event() - Constructor
     *
     * Create an object, but do not mark any particular time
     */
    XCL_DRIVER_DLLESPEC
    user_event() ;

    /**
     * ~user_event() - Destructor
     *
     * Destroy the object, but do not mark any particular time
     */
    XCL_DRIVER_DLLESPEC
    ~user_event() ;
    
    /**
     * mark() - Mark a specific moment in time with a marker on the waveform
     */
    XCL_DRIVER_DLLESPEC
    void mark(const char* label = nullptr) ;
  } ;

} // end namespace profile
} // end namespace xrt

extern "C" {
#endif

/**
 * xrtURStart() - Mark the start time in the user code of a range
 *
 * @id:      A user supplied id to match starts and ends of ranges
 * @label:   The text to display in the waveform for this particular range
 * @tooltip: The text to display in the waveform when hovering over this range
 * Return:   none
 *
 */
XCL_DRIVER_DLLESPEC
void xrtURStart(unsigned int id, const char* label, const char* tooltip) ;

/**
 * xrtUREnd() - Mark the end time in the user code of a range
 *
 * @id:      A user supplied id to match starts and ends of ranges
 * Return:   none
 *
 */
XCL_DRIVER_DLLESPEC
void xrtUREnd(unsigned int id) ;

/**
 * xrtUEMark() - Mark the current time as when something happened
 *
 * @label:   An optional label that is added to the marker in the waveform
 * Return:   none
 *
 */
XCL_DRIVER_DLLESPEC
void xrtUEMark(const char* label) ;

#ifdef __cplusplus
}
#endif

#endif
