/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef USER_EVENT_CALLBACKS_DOT_H
#define USER_EVENT_CALLBACKS_DOT_H

// These are the functions that are visible when the plugin is dynamically
//  linked in.  XRT should call them directly
extern "C"
void user_event_start_cb(unsigned int functionID, 
			 const char* label, 
			 const char* tooltip) ;

extern "C"
void user_event_end_cb(unsigned int functionID) ;

extern "C"
void user_event_happened_cb(const char* label) ;

#endif
