/*****************************************************************************
 *
 *     Author: Xilinx, Inc.
 *
 *     This text contains proprietary, confidential information of
 *     Xilinx, Inc. , is distributed by under license from Xilinx,
 *     Inc., and may be used, copied and/or disclosed only pursuant to
 *     the terms of a valid license agreement with Xilinx, Inc.
 *
 *     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"
 *     AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND
 *     SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,
 *     OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,
 *     APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION
 *     THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,
 *     AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE
 *     FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY
 *     WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE
 *     IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR
 *     REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF
 *     INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE.
 *
 *     Xilinx products are not intended for use in life support appliances,
 *     devices, or systems. Use in such applications is expressly prohibited.
 *
 *     (c) Copyright 2011 Xilinx Inc.
 *     All rights reserved.
 *
 *****************************************************************************/

/*
 * This file contains macro declarations for AutoESL interface directives.
 * Interface directives in the form of macros or pragmas allows the user
 * to tell AutoESL what kind of hardware interface is required.
 */

#ifndef __AP__INTERFACES__
#define __AP__INTERFACES__

//#include "ap_stream.h"

/*
 * Enum of available interfaces in AUTOESL
 */
enum AP_AUTO_INTERFACES {AP_NONE,AP_ACK,AP_VLD,AP_OVLD,AP_HS,AP_CTRL_NONE,AP_CTRL_HS,AP_MEM,AP_FIFO,AP_BUS};

/*
 * GCC pre-processor directive to insert pragmas from macro code
 * AutoESL requires that all hardware centric commands be either in the
 * form of a directive or pragma before hardware synthesis.
 */ 
#define PRAGMA_L1(x) _Pragma(#x)
#define STR(s) #s
#define PRAGMA(x) PRAGMA_L1(x)

/*
 * Allows the user to define the type of AP_INTERFACE to be used.
 * AutoESL generated block in an EDK environment.
 */
/* Array streaming required by AP_HS on arrays */
#define AP_ARRAY_STREAMING(var_name){		\
    PRAGMA(AP array_stream variable=var_name);	\
}

/* Set the unregistered interface */
#define AP_INTERFACE(var_name,interface_type){	\
    PRAGMA(AP interface interface_type port=var_name);	\
  }

/* Set a registered interface */
#define AP_INTERFACE_REG(var_name,interface_type){	\
    PRAGMA(AP interface interface_type port=var_name register);	\
  }

/* Special case for array streaming using ap_hs */
#define AP_ARRAY_STREAM(var_name){		\
    AP_ARRAY_STREAMING(var_name);		\
    AP_INTERFACE(var_name,ap_hs);		\
  }

/*
 * Allows the user to define their own bus type for connection of the 
 * AutoESL generated block in an EDK environment.
 */
/* Basic Definition of a User Declared Bus */
#define AP_BUS_DEFINE(bus_name,bus_standard,bus_type_name){		\
    PRAGMA(AP resource core=bus_name variable=void type=adapter metadata=STR(-bus_std bus_standard -bus_type bus_type_name)); \
  }

/* Connects a user defined bus to a function port variable */
#define AP_BUS_ATTACH(bus_name, var_name){				\
    PRAGMA(AP resource core=bus_name variable=var_name metadata=STR(-bus_bundle)); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_HS */
#define AP_BUS_MAP_HS(bus_name,var_name,source_port,data_port,sync_port1,sync_port2){\
     PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port data_port} {source_port##_ap_vld sync_port1} {source_port##_ap_ack sync_port2}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_FIFO on an input */
#define AP_BUS_MAP_FIFO_IN(bus_name,var_name,source_port,data_port,sync_port1,sync_port2){\
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port##_dout data_port} {source_port##_empty_n sync_port1} {source_port##_read sync_port2}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_FIFO on an output */
#define AP_BUS_MAP_FIFO_OUT(bus_name,var_name,source_port,data_port,sync_port1,sync_port2){\
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port##_din data_port} {source_port##_full_n sync_port1} {source_port##_write sync_port2}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_VLD */
#define AP_BUS_MAP_VLD(bus_name,var_name,source_port,data_port,sync_port){ \
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port data_port} {source_port##_ap_vld sync_port}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_OVLD */
#define AP_BUS_MAP_OVLD(bus_name,var_name,source_port,data_port,sync_port){ \
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port data_port} {source_port##_ap_vld sync_port}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_ACK */
#define AP_BUS_MAP_ACK(bus_name,var_name,source_port,data_port,sync_port){ \
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port data_port} {source_port##_ap_ack sync_port}}); \
  }

/* Provides mapping of ports when the basic AutoESL interfae used is AP_NONE */
#define AP_BUS_MAP_NONE(bus_name,var_name,source_port,data_port){	\
    PRAGMA(AP resource core=bus_name variable=var_name port_map={{source_port data_port}}); \
  }

/* Declaration of a function control bus */
#define AP_CONTROL_BUS(bus_name,bus_standard,bus_type_name){		\
    AP_BUS_DEFINE(bus_name,bus_standard,bus_type_name);			\
    AP_BUS_ATTACH(bus_name,return);					\
    PRAGMA(AP resource core=bus_name variable=return port_map={{ap_start START} {ap_done DONE} {ap_idle IDLE} {ap_return RETURN}}); \
  }

/* Declaration of a user custom data bus*/
#define AP_DATA_BUS(var_name,bus_name,bus_standard,bus_type_name){	\
    AP_BUS_DEFINE(bus_name,bus_standard,bus_type_name);			\
    AP_BUS_ATTACH(bus_name,var_name);					\
  }

/*
 * Create Standard XILINX bus interfaces
 */

/* AXI4 Interfaces - require axi4 library to be loaded in solution */
/* Create an AXI4 Lite interface at the system level layer */
#define AP_BUS_AXI4_LITE(var_name,bus_name){					\
    PRAGMA(AP resource core=AXI_SLAVE variable=var_name metadata=STR(-bus_bundle bus_name)); \
  }

/* Create an AXI4 master interface at the system level layer */
#define AP_BUS_AXI4(var_name){			\
    PRAGMA(AP resource core=xilaxi4_bus_rw variable=var_name);	\
  }

/* Create a basic AXI4 stream interface at the system level layer */
#define AP_BUS_AXI_STREAM(var_name,bus_name){					\
    AP_INTERFACE(var_name,ap_fifo);					\
    PRAGMA(AP resource core=AXIS variable=var_name metadata=STR(-bus_bundle bus_name)); \
    PRAGMA(AP resource core=AXIS variable=var_name port_map={{var_name TDATA}}); \
  }

/* Create an AXI4 stream input interface at the system level layer */
#define AP_BUS_AXI_STREAMD(var_name,bus_name){					\
    AP_INTERFACE(var_name,ap_fifo);					\
    PRAGMA(AP resource core=AXIS variable=var_name metadata=STR(-bus_bundle bus_name)); \
    PRAGMA(AP resource core=AXIS variable=var_name port_map={{var_name##_data_V TDATA} {var_name##_strb_V TSTRB} {var_name##_user_V TUSER} {var_name##_last_V TLAST} {var_name##_tid_V TID} {var_name##_tdest_V TDEST}}); \
  }

/* Create an FSL interface at the system level layer */
#define AP_BUS_FSL(var_name,bus_name){				\
    AP_INTERFACE(var_name,ap_fifo);					\
    PRAGMA(AP resource core=FSL variable=var_name); \
  }

/* Create a PLBv46 slave interface at the system level layer */
#define AP_BUS_PLB_SLAVE(var_name,bus_name){				\
    PRAGMA(AP resource core=PLB_SLAVE variable=var_name metadata=STR(-bus_bundle bus_name)); \
  }

/* Create a PLBv46 master interface at the system level layer */
#define AP_BUS_PLB_MASTER(var_name){				\
    PRAGMA(AP resource core=xilplb46_bus_rw variable=var_name);	\
  }

/* Create an NPI interface at the system level layer */
#define AP_BUS_NPI(var_name){			\
    PRAGMA(AP resource core=npi64 variable=var_name);	\
  }

#endif

