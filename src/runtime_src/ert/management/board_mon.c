/*
 * board_mon.c
 *
 *  Created on: Oct 3, 2016
 *      Author: elleryc
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <mb_interface.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xiic.h"
#include "xgpio.h"
#include "sleep.h"
#include "xuartlite_l.h"
#include "xbram_hw.h"
#include "xintc_l.h"

#define ERT_UNUSED __attribute__((unused))

//FIXED_BOARD should follow vbnv format: xilinx:vcu1525:4ddr-xpr:4.2
#ifdef FIXED_BOARD
#define USE_FIXED_BOARD 1
#else
#define USE_FIXED_BOARD 0
#define FIXED_BOARD ""
#endif

#define VBNV_SEPARATOR "_"

//Must set flag to enable print outs
#ifndef ERT_VERBOSE
#define xil_printf(c, ...) fake_xil_printf(c)
#endif

void fake_xil_printf(char* somestring, ...) {
	return; //Dont actually print out
}

// Please bump uo the patchlevel every time you update the file
#define BOARD_MON_MAJOR 2017
#define BOARD_MON_MINOR 4
#define BOARD_MON_PATCHLEVEL 3

//Set version number based on commit # in repo
#define BOARD_MON_VERSION_NUM (BOARD_MON_MAJOR * 1000 + BOARD_MON_MINOR * 100 + BOARD_MON_PATCHLEVEL)

//Cast functions that preserve bits
#define UNSIGNED_32BIT_CAST(val) *(u32 *)&val
#define SIGNED_32BIT_CAST(val) *(long *)&val

// Board info constants
#define MAX_BOARD_INFO_LENGTH 64 //Gotten from xclfeatures.h
#define VBNV_OFFSET 86

//Register definitions
#define NUM_REGISTERS 35
//Version Register
#define VERSION_REG 0
#define VERSION_REG_ADDR            0x0000
//ID Register
#define ID_REG 1
#define ID_REG_ADDR                 0x0004
#define ID_STRING                   0x74736574
//Status Register
#define STATUS_REG 2
#define STATUS_REG_ADDR             0x0008
#define INIT_SUCCESS                0x00000001
#define MB_STOPPED                  0x00000002
#define MB_PAUSED                   0x00000003
//Error and warnings Register
//Warnings will self clear on read, errors will remain until the reset is set to clear
#define ERROR_REG 3
#define ERROR_REG_ADDR              0x000C
#define TEMP_WARN                   0x00000001
#define TEMP_CRIT_ERROR             0x00000002
#define VCCINT_CUR_WARN             0x00000004
#define VCC1V8_CUR_WARN             0x00000008
#define VCC1V2_CUR_WARN             0x00000010
#define VCCBRAM_CUR_WARN            0x00000020
#define VCCAVCC_CUR_WARN            0x00000040
#define VCCAVTT_CUR_WARN            0x00000080
#define MSP432_UART_ERROR           0x08000000
#define FEATURE_ROM_ERROR           0x10000000
#define CLOCK_CONFIG_ERROR          0x20000000
#define I2C_ALERT                   0x40000000
#define I2C_COMM_ERROR              0x80000000
//Feature support Register
#define FEATURES_REG 4
#define FEATURES_REG_ADDR           0x0010
#define POWMON_SUPPORT              0x00000001
#define BMC_COMM_SUPPORT            0x00000002
#define CLOCK_SCALE_SUPPORT         0x00000004
#define MGTAVTT_AVAILABLE           0x00010000
#define MGTAVCCC_AVAILABLE          0x00020000
#define VCCBRAM_AVAILABLE           0x00040000
#define VCC1V2_AVAILABLE            0x00080000
#define VCC1V8_AVAILABLE            0x00100000
#define VCCINT_AVAILABLE            0x00200000
#define PEX12V_AVAILABLE            0x00400000
#define AUX12V_AVAILABLE            0x00800000
#define PEX3V3_AVAILABLE            0x01000000
//Reset and control register
#define CONTROL_REG 5
#define CONTROL_REG_ADDR            0x0018
#define RESET_CUR_READINGS          0x00000001 //Clears average and max readings for current
#define RESET_ERROR_FLAGS           0x00000002 //Clears error flags
#define PAUSE_MB                    0x00000004 //Self clears after 10s
#define STOP_MB                     0x00000008 //Request to stop microblaze. Must set confirmation bit
#define UPDATE_MMCMS                0x00000010 //Update MMCM outputs
//Stop confirmation register
#define STOP_MB_CONFIRM_REG 6
#define STOP_MB_CONFIRM_REG_ADDR    0x001C
#define STOP_MB_CONFIRM             0x00000001 //Must be set for microblaze to stop
//VCCINT Registers
#define VCCINT_CUR_MAX_REG 7
#define VCCINT_CUR_MAX_ADDR         0x0020
#define VCCINT_CUR_AVG_REG 8
#define VCCINT_CUR_AVG_ADDR         0x0024
#define VCCINT_CUR_INS_REG 9
#define VCCINT_CUR_INS_ADDR         0x0028
//VCC1V8 Registers
#define VCC1V8_CUR_MAX_REG 10
#define VCC1V8_CUR_MAX_ADDR         0x002C
#define VCC1V8_CUR_AVG_REG 11
#define VCC1V8_CUR_AVG_ADDR         0x0030
#define VCC1V8_CUR_INS_REG 12
#define VCC1V8_CUR_INS_ADDR         0x0034
//VCC1V2 Registers
#define VCC1V2_CUR_MAX_REG 13
#define VCC1V2_CUR_MAX_ADDR         0x0038
#define VCC1V2_CUR_AVG_REG 14
#define VCC1V2_CUR_AVG_ADDR         0x003C
#define VCC1V2_CUR_INS_REG 15
#define VCC1V2_CUR_INS_ADDR         0x0040
//VCCBRAM Registers
#define VCCBRAM_CUR_MAX_REG 16
#define VCCBRAM_CUR_MAX_ADDR        0x0044
#define VCCBRAM_CUR_AVG_REG 17
#define VCCBRAM_CUR_AVG_ADDR        0x0048
#define VCCBRAM_CUR_INS_REG 18
#define VCCBRAM_CUR_INS_ADDR        0x004C
//VCCAVCC Registers
#define VCCAVCC_CUR_MAX_REG 19
#define VCCAVCC_CUR_MAX_ADDR        0x0050
#define VCCAVCC_CUR_AVG_REG 20
#define VCCAVCC_CUR_AVG_ADDR        0x0054
#define VCCAVCC_CUR_INS_REG 21
#define VCCAVCC_CUR_INS_ADDR        0x0058
//VCCAVTT Registers
#define VCCAVTT_CUR_MAX_REG 22
#define VCCAVTT_CUR_MAX_ADDR        0x005C
#define VCCAVTT_CUR_AVG_REG 23
#define VCCAVTT_CUR_AVG_ADDR        0x0060
#define VCCAVTT_CUR_INS_REG 24
#define VCCAVTT_CUR_INS_ADDR        0x0064
//PEXV12 Registers
#define PEXV12_CUR_MAX_REG 25
#define PEXV12_CUR_MAX_ADDR        0x0068
#define PEXV12_CUR_AVG_REG 26
#define PEXV12_CUR_AVG_ADDR        0x006C
#define PEXV12_CUR_INS_REG 27
#define PEXV12_CUR_INS_ADDR        0x0070
//AUX12V Registers
#define AUX12V_CUR_MAX_REG 28
#define AUX12V_CUR_MAX_ADDR        0x0074
#define AUX12V_CUR_AVG_REG 29
#define AUX12V_CUR_AVG_ADDR        0x0078
#define AUX12V_CUR_INS_REG 30
#define AUX12V_CUR_INS_ADDR        0x007C
//PEX3V3 Registers
#define PEX3V3_CUR_MAX_REG 31
#define PEX3V3_CUR_MAX_ADDR        0x0080
#define PEX3V3_CUR_AVG_REG 32
#define PEX3V3_CUR_AVG_ADDR        0x0084
#define PEX3V3_CUR_INS_REG 33
#define PEX3V3_CUR_INS_ADDR        0x0088
//Power Checksum Register
#define CUR_CHKSUM_REG      34
#define CUR_CHKSUM_ADDR             0x01A4
/* TODO: If supported make sure to update register indices
//Kernel MMCM0 Registers
#define KERNEL_MMCM0_STAT_REG 26
#define KERNEL_MMCM0_STAT_ADDR      0x01B0
#define KERNEL_MMCM0_CONF_REG 27
#define KERNEL_MMCM0_CONFIG_ADDR    0x01B4
#define KERNEL_MMCM0_OUT1_REG 28
#define KERNEL_MMCM0_OUT1_ADDR      0x01B8
#define KERNEL_MMCM0_OUT2_REG 29
#define KERNEL_MMCM0_OUT2_ADDR      0x01BC
#define KERNEL_MMCM0_OUT3_REG 30
#define KERNEL_MMCM0_OUT3_ADDR      0x01C0
#define KERNEL_MMCM0_OUT4_REG 31
#define KERNEL_MMCM0_OUT4_ADDR      0x01C4
#define KERNEL_MMCM0_OUT5_REG 32
#define KERNEL_MMCM0_OUT5_ADDR      0x01C8
//Kernel MMCM1 Registers
#define KERNEL_MMCM1_STAT_REG 33
#define KERNEL_MMCM1_STAT_ADDR      0x01CC
#define KERNEL_MMCM1_CONF_REG 34
#define KERNEL_MMCM1_CONFIG_ADDR    0x01D0
#define KERNEL_MMCM1_OUT1_REG 35
#define KERNEL_MMCM1_OUT1_ADDR      0x01D4
#define KERNEL_MMCM1_OUT2_REG 36
#define KERNEL_MMCM1_OUT2_ADDR      0x01D8
#define KERNEL_MMCM1_OUT3_REG 37
#define KERNEL_MMCM1_OUT3_ADDR      0x01DC
#define KERNEL_MMCM1_OUT4_REG 38
#define KERNEL_MMCM1_OUT4_ADDR      0x01E0
#define KERNEL_MMCM1_OUT5_REG 39
#define KERNEL_MMCM1_OUT5_ADDR      0x01E4
*/

//VU9P_HP Definitions
#define VU9P_HP_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define VU9P_HP_NUM_SUPPLIES 	6
//VU9P Definitions
#define VU9P_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define VU9P_NUM_SUPPLIES 	5
//KU115 Definitions
#define KU115_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define KU115_NUM_SUPPLIES 	5
//KCU1500 Definitions
#define KCU1500_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define KCU1500_NUM_SUPPLIES 	5
//VCU1525 Definitions - Read from MSP432
//#define VCU1525_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define VCU1525_NUM_SUPPLIES 	4
#define VCCINT_MULT_FACTOR 6 //6 phase supply
//VCU1526 Definitions
#define VCU1526_IIC_MUX_ADDR 	0x74 //This is the true 7bit address
#define VCU1526_NUM_SUPPLIES 	1

//When host requests to read power measurements the microblaze will pause for 10s
#define MB_PAUSED_TIMEOUT_US 10000000 //10s timeout

// PMBUS COMMANDS
#define READ_VOUT				0x8B
#define	READ_IOUT				0x8C

// I2C MUX Selection and slave addresses
#define DISABLE_MUX 				0x00
#define PMBUS_SEL 					0x01

// Status from store commands
#define STORE_SUCCESS 0
#define STORE_FAILED 1
#define STORE_OVERFLOW 2

// Regulator IDs
#define MAX15301 1
#define MAX20751 2
#define LTC3884 3

// Current sensor effective resistance (1/mOhms)
// Reff = Rin/(Rout*Rsense) * 1000
#define VCU1525_LTC6103_REFF  4000 
#define VCU1525_LTC6106_REFF  8032

// Interrupts
//TODO: bits 0-5 are sysmon interrupts
#define MSP432_UART_INT (1<<6)

// MSP432 Interface Definitions and sizes
#define ASCII_STX 0x02 //Start of transmission packet from MSP432
#define ASCII_ETX 0x03 //End of transmission packet from MSP432

#define EEPROM_BOARD_NAME_SIZE         0x10
#define EEPROM_BOARD_REV_SIZE          0x04
#define EEPROM_BOARD_SERIAL_SIZE       0x20
#define EEPROM_BOARD_MAC_SIZE          0x07
#define IPMI_REV_SIZE                  0x01
#define FW_REV_SIZE                    0x01
#define BOARD_INFO_SIZE                 (EEPROM_BOARD_NAME_SIZE + EEPROM_BOARD_REV_SIZE + \
                                        EEPROM_BOARD_SERIAL_SIZE + EEPROM_BOARD_MAC_SIZE + \
                                        (IPMI_REV_SIZE * 2/* major + minor */) + \
                                        (FW_REV_SIZE * 2/* major + minor */))

#define RD_CODE_LTC3884_CURRENT_CH1   0x12
#define SENSOR_ADC_NUM                16 
#define SENSOR_SE98A_NUM              3
#define SENSOR_LTC3884_NUM            4
                                       
#define NUM_ADC_READINGS               (RD_CODE_LTC3884_CURRENT_CH1 + SENSOR_ADC_NUM)
#define SE98A_DATA_SIZE               sizeof(float)
#define LM96063_LOCAL_TEMP_DATA_SIZE  sizeof(float)
#define LM96063_REMOTE_TEMP_DATA_SIZE sizeof(int32_t)
#define LM96063_FANRPM_DATA_SIZE      sizeof(uint8_t)
#define LTC3884_DATA_SIZE             sizeof(float)
#define ADC_DATA_SIZE                 sizeof(uint32_t)

#define UART_PUSH_DATA_SIZE           (1 + NUM_ADC_READINGS + \
                                       (SENSOR_SE98A_NUM * SE98A_DATA_SIZE) + \
                                       LM96063_LOCAL_TEMP_DATA_SIZE + \
                                       LM96063_REMOTE_TEMP_DATA_SIZE + \
                                       LM96063_FANRPM_DATA_SIZE + \
                                       (SENSOR_LTC3884_NUM * LTC3884_DATA_SIZE) + \
                                       (SENSOR_ADC_NUM * ADC_DATA_SIZE) + \
                                       BOARD_INFO_SIZE + \
                                       1 /* Checksum byte */)

#define ADC_SENSOR_OFFSET              (1 + NUM_ADC_READINGS + \
                                       (SENSOR_SE98A_NUM * SE98A_DATA_SIZE) + \
                                       LM96063_LOCAL_TEMP_DATA_SIZE + \
                                       LM96063_REMOTE_TEMP_DATA_SIZE + \
                                       LM96063_FANRPM_DATA_SIZE + \
                                       BOARD_INFO_SIZE)
                                                                        
#define PEXV12_I_IN_PKT_OFFSET          (1 + ADC_SENSOR_OFFSET + (14*(ADC_DATA_SIZE+1)))
#define AUX_12V_I_IN_PKT_OFFSET         (1 + ADC_SENSOR_OFFSET + (15*(ADC_DATA_SIZE+1)))
#define PEX3V3_I_IN_PKT_OFFSET          (1 + ADC_SENSOR_OFFSET + (13*(ADC_DATA_SIZE+1)))
                                       
#define MAX_SENSOR_DATA_RCV_SIZE UART_PUSH_DATA_SIZE
//End MSP432 definitions

//Used for debug printouts of measured voltages/currents
char *sensor_data_print_fmt[] =
{
		"", /* dummy */
		"BOARD_NAME: %s\n", "BOARD_REV: %d\n", "BOARD_SERIAL: %s\n",
		"BOARD_MAC: %s\n", "", "IPMI_VER: %d.%d\n", "", "FW_VER: %d.%d\n",
		"SE98A_1 - Temperature: %f\n", "SE98A_2 - Temperature: %f\n",
		"SE98A_3 - Temperature: %f\n",
		"LM96063 - Local Temperature: %d\n",
		"LM96063 - Remote Temperature: %f\n", "LM96063 - FAN RPM: %d\n",
		"LTC3884 - CH0 Voltage: %f\n", "LTC3884 - CH1 Voltage: %f\n",
		"LTC3884 - CH0 Current: %f\n", "LTC3884 - CH1 Current: %f\n",
		"12V_PEX: %u\n", "3V3_PEX: %u\n", "3V3AUX: %u\n",
		"12V_AUX: %u\n", "DDR4_VPP_BTM: %u\n", "SYS_5V5: %u\n",
		"VCC1V2_TOP: %u\n", "VCC1V8: %u\n", "VCC0V85: %u\n",
		"DDR4_VPP_TOP: %u\n", "MGT0V9AVCC: %u\n", "12V_SW: %u\n",
		"MGTAVTT: %u\n", "PEX3V3_I_IN: %u\n", "PEXV12_I_IN: %u\n",
		"12V_AUX_I_IN: %u\n"
};

// Register struct
typedef struct {
    u16 addr;
    u32 reg_val;
} Register;

//Supply info struct
typedef struct {
	s32 sum_iout; //format is mamps
	Register *max_iout_reg;
	Register *avg_iout_reg;
	Register *cur_iout_reg;
	const char *supply_name;
	const u8 IIC_ADDR; //Used for PMBUS sensor data
	const u8 chipid; //Used for PMBUS sensor data
	const u8 sensor_byte_addr; //Used for MSP432 sensor data
	//Resistor value used to convert Volts to Amps. Reff_inv = Rin/(Rout*Rsense) * 1000
	//Units are in Ohms scaled up 1k
	const u32 cur_sense_reff_inv; 
} SupplyStats;

//Board info struct
typedef struct  {
    //WARNING: Since the BRAM is word indexed there may be a byte offset required
    //to get the start of the VBNV string. Not required if not reading from BRAM
    //ex: _board_info.vbnv_info[VBNV_OFFSET%4]
	char vbnv_info[MAX_BOARD_INFO_LENGTH]; 
	char *vendor, *board, *name, *version;
	u8 iic_mux_addr;
	SupplyStats *supplies;
	u8 num_supplies;
} BoardInfo;

BoardInfo _board_info;

//Register Map
Register RegisterMap[NUM_REGISTERS] = {
    {.addr = VERSION_REG_ADDR,          .reg_val = BOARD_MON_VERSION_NUM},
    {.addr = ID_REG_ADDR,               .reg_val = ID_STRING},
    {.addr = STATUS_REG_ADDR,           .reg_val = 0},
    {.addr = ERROR_REG_ADDR,            .reg_val = 0},
    {.addr = FEATURES_REG_ADDR,         .reg_val = 0},
    {.addr = CONTROL_REG_ADDR,          .reg_val = 0},
    {.addr = STOP_MB_CONFIRM_REG_ADDR,  .reg_val = 0},
    {.addr = VCCINT_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = VCCINT_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = VCCINT_CUR_INS_ADDR,       .reg_val = 0},
    {.addr = VCC1V8_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = VCC1V8_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = VCC1V8_CUR_INS_ADDR,       .reg_val = 0},
    {.addr = VCC1V2_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = VCC1V2_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = VCC1V2_CUR_INS_ADDR,       .reg_val = 0},
    {.addr = VCCBRAM_CUR_MAX_ADDR,      .reg_val = 0},
    {.addr = VCCBRAM_CUR_AVG_ADDR,      .reg_val = 0},
    {.addr = VCCBRAM_CUR_INS_ADDR,      .reg_val = 0},
    {.addr = VCCAVCC_CUR_MAX_ADDR,      .reg_val = 0},
    {.addr = VCCAVCC_CUR_AVG_ADDR,      .reg_val = 0},
    {.addr = VCCAVCC_CUR_INS_ADDR,      .reg_val = 0},
    {.addr = VCCAVTT_CUR_MAX_ADDR,      .reg_val = 0},
    {.addr = VCCAVTT_CUR_AVG_ADDR,      .reg_val = 0},
    {.addr = VCCAVTT_CUR_INS_ADDR,      .reg_val = 0},
    {.addr = PEXV12_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = PEXV12_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = PEXV12_CUR_INS_ADDR,       .reg_val = 0},
    {.addr = AUX12V_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = AUX12V_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = AUX12V_CUR_INS_ADDR,       .reg_val = 0},
    {.addr = PEX3V3_CUR_MAX_ADDR,       .reg_val = 0},
    {.addr = PEX3V3_CUR_AVG_ADDR,       .reg_val = 0},
    {.addr = PEX3V3_CUR_INS_ADDR,       .reg_val = 0},            
    {.addr = CUR_CHKSUM_ADDR,           .reg_val = 0}/*,
    {.addr = KERNEL_MMCM0_STAT_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM0_CONFIG_ADDR,  .reg_val = 0},
    {.addr = KERNEL_MMCM0_OUT1_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM0_OUT2_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM0_OUT3_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM0_OUT4_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM0_OUT5_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_STAT_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_CONFIG_ADDR,  .reg_val = 0},
    {.addr = KERNEL_MMCM1_OUT1_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_OUT2_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_OUT3_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_OUT4_ADDR,    .reg_val = 0},
    {.addr = KERNEL_MMCM1_OUT5_ADDR,    .reg_val = 0}*/
};

//Supply structs for each board
static SupplyStats VU9P_HP_SUPPLIES[VU9P_HP_NUM_SUPPLIES] = {
		{.supply_name="VCCINT",  	.IIC_ADDR=0x50, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V8",  	.IIC_ADDR=0x14, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V8_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V8_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V8_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V2",  	.IIC_ADDR=0x12, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V2_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V2_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V2_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCCBRAM", 	.IIC_ADDR=0x0D, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCCBRAM_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCBRAM_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCBRAM_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVCC",  	.IIC_ADDR=0x72, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVCC_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVCC_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVCC_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVTT",  	.IIC_ADDR=0x73, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVTT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVTT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVTT_CUR_INS_REG], .sum_iout=0}
};

static SupplyStats VU9P_SUPPLIES[VU9P_NUM_SUPPLIES] = {
		{.supply_name="VCCINT",  	.IIC_ADDR=0x0A, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V8",  	.IIC_ADDR=0x14, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V8_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V8_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V8_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V2",  	.IIC_ADDR=0x12, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V2_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V2_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V2_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVCC",  	.IIC_ADDR=0x72, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVCC_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVCC_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVCC_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVTT",  	.IIC_ADDR=0x73, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVTT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVTT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVTT_CUR_INS_REG], .sum_iout=0}
};

static SupplyStats KU115_SUPPLIES[KU115_NUM_SUPPLIES] = {
		{.supply_name="VCCINT",  	.IIC_ADDR=0x0A, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V8",  	.IIC_ADDR=0x14, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V8_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V8_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V8_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V2",  	.IIC_ADDR=0x12, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V2_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V2_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V2_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVCC",  	.IIC_ADDR=0x72, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVCC_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVCC_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVCC_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVTT",  	.IIC_ADDR=0x73, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVTT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVTT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVTT_CUR_INS_REG], .sum_iout=0}
};

static SupplyStats KCU1500_SUPPLIES[KCU1500_NUM_SUPPLIES] = {
		{.supply_name="VCCINT",  	.IIC_ADDR=0x0A, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V8",  	.IIC_ADDR=0x14, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V8_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V8_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V8_CUR_INS_REG], .sum_iout=0},
		{.supply_name="VCC1V2",  	.IIC_ADDR=0x12, .chipid=MAX15301, .max_iout_reg=&RegisterMap[VCC1V2_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCC1V2_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCC1V2_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVCC",  	.IIC_ADDR=0x72, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVCC_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVCC_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVCC_CUR_INS_REG], .sum_iout=0},
		{.supply_name="MGTAVTT",  	.IIC_ADDR=0x73, .chipid=MAX20751, .max_iout_reg=&RegisterMap[VCCAVTT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCAVTT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCAVTT_CUR_INS_REG], .sum_iout=0}
};

static SupplyStats VCU1525_SUPPLIES[VCU1525_NUM_SUPPLIES] = {
		/*{.supply_name="VCCINT",  	.IIC_ADDR=0x44, .chipid=LTC3884, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0}*/
	    {.supply_name="PEXV12",  	 .cur_sense_reff_inv=VCU1525_LTC6103_REFF, .sensor_byte_addr=PEXV12_I_IN_PKT_OFFSET, .max_iout_reg=&RegisterMap[PEXV12_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[PEXV12_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[PEXV12_CUR_INS_REG], .sum_iout=0},
		{.supply_name="AUX12V",  	 .cur_sense_reff_inv=VCU1525_LTC6106_REFF, .sensor_byte_addr=AUX_12V_I_IN_PKT_OFFSET, .max_iout_reg=&RegisterMap[AUX12V_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[AUX12V_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[AUX12V_CUR_INS_REG], .sum_iout=0},
		{.supply_name="PEX3V3",  	 .cur_sense_reff_inv=VCU1525_LTC6103_REFF, .sensor_byte_addr=PEX3V3_I_IN_PKT_OFFSET, .max_iout_reg=&RegisterMap[PEX3V3_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[PEX3V3_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[PEX3V3_CUR_INS_REG], .sum_iout=0}
};

static SupplyStats VCU1526_SUPPLIES[VCU1526_NUM_SUPPLIES] = {
		{.supply_name="VCCINT",  	.IIC_ADDR=0x44, .chipid=LTC3884, .max_iout_reg=&RegisterMap[VCCINT_CUR_MAX_REG],
		    .avg_iout_reg=&RegisterMap[VCCINT_CUR_AVG_REG], .cur_iout_reg=&RegisterMap[VCCINT_CUR_INS_REG], .sum_iout=0}
};

//Global variables
u32 num_samps = 0; //Number of samples taken used for averaging
u32 bram = 0; //BRAM address offset
u32 intc = 0; //Interrupt controller
u32 uart = 0; //UART device for MSP432 communication
bool msp432_support = false; //Not supported for all platforms
volatile bool msp432_synced = false; //Initially when we read from UART it will be in the middle of a packet, so throw away data until next start char
volatile u16 rcv_idx = 0; //Receive index for uart buffer
volatile int store_flag = STORE_SUCCESS;
volatile uint8_t uart_buffer[MAX_SENSOR_DATA_RCV_SIZE*2]; //Encoding is 2x the size of packet
volatile uint16_t *sensor_ascii_data;
uint8_t sensor_rcv_data[MAX_SENSOR_DATA_RCV_SIZE];

// function headers
int iic_mux_select (u32 IIC, u8 iic_mux_chan);
int reset_average_value(u32 IIC, SupplyStats *supply);
int pmbus_read(u32 IIC, u8 dev_addr, u8 command, u8 *rxBuf);
void pmbus_reset(u32 IIC);
void pmbus_print_voltage(u32 IIC, SupplyStats *supply);
void pmbus_print_current(u32 IIC, SupplyStats *supply);
int store_current(u32 IIC, SupplyStats *supply);
void write_cur_checksum();
int init_board_info(u32 feature_rom);
void write_reg(Register *reg, u32 val);
u32 read_reg(Register *reg);
// End function headers

void write_cur_checksum() {
	u32 cur_checksum = 0;
	for(u8 i=0; i<_board_info.num_supplies; i++) {
		cur_checksum += _board_info.supplies[i].max_iout_reg->reg_val;
		cur_checksum += _board_info.supplies[i].avg_iout_reg->reg_val;
		cur_checksum += _board_info.supplies[i].cur_iout_reg->reg_val;
	}

	write_reg(&RegisterMap[CUR_CHKSUM_REG], cur_checksum);
}

int iic_mux_select(u32 IIC, u8 iic_mux_chan){
	if(sizeof(iic_mux_chan) != XIic_Send(IIC, _board_info.iic_mux_addr, &iic_mux_chan, sizeof(iic_mux_chan), XIIC_STOP)) {
	    xil_printf("Failed to set IIC Mux!\n");
	    write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val | I2C_COMM_ERROR);
	    return XST_FAILURE;
	}
	xil_printf("IIC Mux Channel PMBUS Selected\n");
	return XST_SUCCESS;
}

u32 convert_bits_to_mv(u16 volts_bits, u8 chip_id) {
	if(chip_id == MAX15301) //V*2^-12
		return (volts_bits * 1000) >> 12;
	else if(chip_id == MAX20751)
		return (volts_bits - 1) * 5 + 250;
	else if(chip_id == LTC3884)
		return (volts_bits * 1000) >> 12;

	return 0;
}

s32 convert_bits_to_mamps(s16 amps_bits) {
	s8 exp = amps_bits >> 11;
	s8 exp_pos = ~(exp) + 1;
	s32 mantissa = amps_bits << 21; mantissa >>= 21; //remove extra bits
	mantissa *= 1000; //convert to milliamps

	return (exp < 0) ? mantissa >> exp_pos : mantissa << exp;
}

u32 convert_msp432_data_to_mamps(u32 uVolts_bits, SupplyStats *supply) {
    u32 mVolts = uVolts_bits/1000; //Scale down to mVolts to prevent potential overflow in next line
	return mVolts*supply->cur_sense_reff_inv / 1000; // mV/Ohm = mA (Rescale by 1k since we scaled effective resistance)
}

u8 get_pmbus_rx_bytes(u8 command) {
	switch(command) {
	case READ_VOUT:
	case READ_IOUT:
		return 2;
	}
	return 0;
}

void pmbus_reset(u32 IIC) {
	xil_printf("Resetting IIC bus.\n");
	XIic_DynInit(IIC);
	usleep(10000); //sleep for 10ms
}

int pmbus_read(u32 IIC, u8 dev_addr, u8 command, u8 *rxBuf) {
	if(sizeof(command) != XIic_Send(IIC, dev_addr, &command, sizeof(command), XIIC_REPEATED_START)) {
    	write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val | I2C_COMM_ERROR);
		xil_printf("IIC write failed!\n");
		pmbus_reset(IIC);
		return XST_FAILURE;
	}
	if(get_pmbus_rx_bytes(command) != XIic_Recv(IIC, dev_addr, rxBuf, get_pmbus_rx_bytes(command), XIIC_STOP)) {
	    write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val | I2C_COMM_ERROR);
		xil_printf("IIC read failed!\n");
		pmbus_reset(IIC);
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

void pmbus_print_voltage(u32 IIC, SupplyStats *supply) {
	u8 rxBuf[32];
	u16 vout_bits;
	u32 vout_mv;
	u32 vout_v;

	if(pmbus_read(IIC, supply->IIC_ADDR, READ_VOUT, rxBuf) != XST_SUCCESS) {
		xil_printf("Failed to read %s!\n", supply->supply_name);
	}else {
		vout_bits = (rxBuf[1] << 8 | rxBuf[0]);
		vout_mv = convert_bits_to_mv(vout_bits, supply->chipid);
		vout_v = vout_mv/1000;
		vout_mv -= vout_v*1000;
		//xil_printf("%s (0x%02X):\t\t 0x%04X\n", supply->supply_name, supply->IIC_ADDR, vout_bits);
		xil_printf("%s (0x%02X):\t\t %d.%dV\n", supply->supply_name, supply->IIC_ADDR, vout_v, vout_mv);
	}
}

void pmbus_print_current(u32 IIC, SupplyStats *supply) {
	u8 rxBuf[32];
	s16 iout_bits;
	s32 iout_ma;
	s32 iout_a;

	if(pmbus_read(IIC, supply->IIC_ADDR, READ_IOUT, rxBuf) != XST_SUCCESS) {
		xil_printf("Failed to read %s!\n", supply->supply_name);
	}else {
		iout_bits = (rxBuf[1] << 8 | rxBuf[0]);
		iout_ma = convert_bits_to_mamps(iout_bits);
		iout_a = iout_ma/1000;
		iout_ma -= iout_a*1000;
		//xil_printf("%s (0x%02X):\t\t 0x%04X\n", supply->supply_name, supply->IIC_ADDR, (u16) iout_bits);
		xil_printf("%s (0x%02X):\t\t %d.%dA\n", supply->supply_name, supply->IIC_ADDR, iout_a, iout_ma);
	}
}

//IIC will be -1 if IIC is not used
int store_current(u32 IIC, SupplyStats *supply) {
	u8 rxBuf[32];
	s16 iout_bits;
	u32 iout_ma;

    if(IIC == -1) { //Get data from MSP432 packet instead of PMBUS
        if(strcmp(supply->supply_name, "VCCINT") == 0) {
            u32 adc_bytes;
            memcpy(&adc_bytes, &sensor_rcv_data[supply->sensor_byte_addr], ADC_DATA_SIZE);
            float *adc_data = (float *)&adc_bytes;
            //xil_printf("%s current: %dmA\n", supply->supply_name, (u32)abs((*adc_data * 1000.0)));
            iout_ma = VCCINT_MULT_FACTOR*((u32)abs((*adc_data * 1000.0)));
        } else {
            u32 adc_data;
		    memcpy(&adc_data, &sensor_rcv_data[supply->sensor_byte_addr], ADC_DATA_SIZE);
            iout_ma = abs(convert_msp432_data_to_mamps(adc_data, supply));
            //xil_printf("%s current: %dmA\n", supply->supply_name, iout_ma);
        }
    } else if(pmbus_read(IIC, supply->IIC_ADDR, READ_IOUT, rxBuf) != XST_SUCCESS) {
		xil_printf("Failed to read %s!\n", supply->supply_name);
		return STORE_FAILED;
	} else {
	    iout_bits = (rxBuf[1] << 8 | rxBuf[0]);
	    iout_ma = abs(convert_bits_to_mamps(iout_bits));
	}

    write_reg(supply->cur_iout_reg, iout_ma);

    if(iout_ma > supply->max_iout_reg->reg_val)
	    write_reg(supply->max_iout_reg, iout_ma);
	    
	supply->sum_iout += iout_ma;

	if(supply->sum_iout < iout_ma) {//overflow, rst sum/avg
		return STORE_OVERFLOW; //return early and do not write to BRAM
	} else {
	    write_reg(supply->avg_iout_reg, supply->sum_iout / num_samps);

		return STORE_SUCCESS;
	}
}

int reset_average_value(u32 IIC, SupplyStats *supply) {
	supply->sum_iout = 0;

	return store_current(IIC, supply);
}

//Update both microblaze register and bram
void write_reg(Register *reg, u32 val) {
    //xil_printf("Writing 0x%08X to 0x%08X\n", val, bram+reg->addr);
    reg->reg_val = val;
    XBram_WriteReg(bram, reg->addr, val);
}

//Protect read only bits by using reg_read_mask
u32 read_reg(Register *reg) {
    reg->reg_val = XBram_ReadReg(bram, reg->addr);
    //xil_printf("Read 0x%08X from 0x%08X\n", reg->reg_val, bram+reg->addr);
    return reg->reg_val;
}

//Store and print sensor data from MSP432. Debug only
void print_sensor_data(void)
{
	uint8_t j, k;
	char board_name[EEPROM_BOARD_NAME_SIZE] = {'\0'};
	uint32_t board_rev;
	char board_serial[EEPROM_BOARD_SERIAL_SIZE] = {'\0'};
	char board_mac[EEPROM_BOARD_MAC_SIZE] = {'\0'};
	uint8_t ipmi_ver_major;
	uint8_t ipmi_ver_minor;
	uint8_t fw_ver_major;
	uint8_t fw_ver_minor;
	ERT_UNUSED float *se98a_reading;
	int32_t lm96063_local_temp;
	ERT_UNUSED float   *lm96063_remote_temp;
	uint8_t lm96063_fan_rpm;
	ERT_UNUSED float *ltc3884_temp;
	uint32_t adc_data;
	uint8_t rd_code;

	j = 1;// first byte is number of readings

	rd_code = sensor_rcv_data[j];
	memcpy(board_name, &sensor_rcv_data[j + 1], EEPROM_BOARD_NAME_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], board_name);
	j += EEPROM_BOARD_NAME_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&board_rev, &sensor_rcv_data[j + 1], EEPROM_BOARD_REV_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], board_rev);
	j += EEPROM_BOARD_REV_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(board_serial, &sensor_rcv_data[j + 1], EEPROM_BOARD_SERIAL_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], board_serial);
	j += EEPROM_BOARD_SERIAL_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(board_mac, &sensor_rcv_data[j + 1], EEPROM_BOARD_MAC_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], board_mac);
	j += EEPROM_BOARD_MAC_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&ipmi_ver_major, &sensor_rcv_data[j + 1], IPMI_REV_SIZE);
	j += IPMI_REV_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&ipmi_ver_minor, &sensor_rcv_data[j + 1], IPMI_REV_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], ipmi_ver_major, ipmi_ver_minor);
	j += IPMI_REV_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&fw_ver_major, &sensor_rcv_data[j + 1], FW_REV_SIZE);
	j += FW_REV_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&fw_ver_minor, &sensor_rcv_data[j + 1], FW_REV_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], fw_ver_major, fw_ver_minor);
	j += FW_REV_SIZE + 1;

	for (k = 0; k < SENSOR_SE98A_NUM; k++)
	{
		rd_code = sensor_rcv_data[j];
		se98a_reading = (float *)&sensor_rcv_data[j + 1];
		//xil_printf(sensor_data_print_fmt[rd_code], se98a_reading[0]);
		j += SE98A_DATA_SIZE + 1;
	}

	rd_code = sensor_rcv_data[j];
	memcpy(&lm96063_local_temp, &sensor_rcv_data[j + 1], LM96063_LOCAL_TEMP_DATA_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], lm96063_local_temp);
	j += LM96063_LOCAL_TEMP_DATA_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	lm96063_remote_temp = (float *)&sensor_rcv_data[j + 1];
	//xil_printf(sensor_data_print_fmt[rd_code], lm96063_remote_temp[0]);
	j += LM96063_REMOTE_TEMP_DATA_SIZE + 1;
	rd_code = sensor_rcv_data[j];
	memcpy(&lm96063_fan_rpm, &sensor_rcv_data[j + 1], LM96063_FANRPM_DATA_SIZE);
	xil_printf(sensor_data_print_fmt[rd_code], lm96063_fan_rpm);
	j += LM96063_FANRPM_DATA_SIZE + 1;

	for (k = 0; k < SENSOR_LTC3884_NUM; k++)
	{
		rd_code = sensor_rcv_data[j];
		ltc3884_temp = (float *)&sensor_rcv_data[j + 1];
		//xil_printf(sensor_data_print_fmt[rd_code], ltc3884_temp[0]);
		j += LTC3884_DATA_SIZE + 1;
	}

	for (k = 0; k < SENSOR_ADC_NUM; k++)
	{
		rd_code = sensor_rcv_data[j];
		memcpy(&adc_data, &sensor_rcv_data[j + 1], ADC_DATA_SIZE);
		xil_printf(sensor_data_print_fmt[rd_code], adc_data);
		j += ADC_DATA_SIZE + 1;
	}
}

//Converts 16b ASCII encoded data to true 8bit value
int ascii_to_data(uint16_t ascii_data, uint8_t *data)
{
	uint8_t h_byte = (ascii_data >> 8);
	uint8_t l_byte = (ascii_data & 0xFF);

    *data = 0;
	if (h_byte >= '0' && h_byte <= '9')
		*data = (h_byte - '0') << 4;
	else if (h_byte >= 'A' && h_byte <= 'F')
		*data = (h_byte-'A'+10) << 4;
	else
	    return XST_FAILURE;
	    
	if (l_byte >= '0' && l_byte <= '9')
		*data |= l_byte - '0';
	else if (l_byte >= 'A' && l_byte <= 'F')
		*data |= l_byte-'A'+10;
	else
	    return XST_FAILURE;

	return XST_SUCCESS;
}

//Note that the input buffer is reinterpeted as 16bit ints
int process_rcvd_sensor_data(uint16_t *buffer) {
	uint8_t i;
    uint8_t rcvd_chksum;
    uint8_t calc_chksum = 0;

    //Convert data and calculate checksum
	for (i = 0; i < MAX_SENSOR_DATA_RCV_SIZE-1; i++)
		if(ascii_to_data(buffer[i],sensor_rcv_data+i) != XST_SUCCESS)
		    return XST_FAILURE;
        else
	        calc_chksum += sensor_rcv_data[i];

    //Get received checksum
	if(ascii_to_data(buffer[MAX_SENSOR_DATA_RCV_SIZE-1],&rcvd_chksum) != XST_SUCCESS)
	    return XST_FAILURE;
	    
	  
	calc_chksum = 0x100-calc_chksum;

	return (rcvd_chksum == calc_chksum) ? XST_SUCCESS : XST_FAILURE;
}

/**
 * Interrupt service routine for UART interrupts
 * WARNING: Be careful of printfs in handler as it can cause overflow in FIFO for UART
 */
void cu_interrupt_handler() __attribute__((interrupt_handler));
void cu_interrupt_handler() {

    if (XIntc_GetIntrStatus(intc) & MSP432_UART_INT) { // uart interrupt
        while(!XUartLite_IsReceiveEmpty(uart)) { //Read through buffer
            u8 rcv_byte = XUartLite_RecvByte(uart);
            //xil_printf("Reading uart: 0x%02X\n", rcv_byte);

            if (rcv_byte == ASCII_STX) {
                rcv_idx = 0;
                msp432_synced = true;
            } else if(rcv_byte == ASCII_ETX && msp432_synced) {
                if(process_rcvd_sensor_data((u16 *)uart_buffer) != XST_SUCCESS) 
                    write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val | MSP432_UART_ERROR);
                else {
                    num_samps+=1;
                    //print_sensor_data(); 
                    for(int i=0;i<_board_info.num_supplies;i++) {
                        store_flag = store_current(-1, &_board_info.supplies[i]);
                        if(store_flag != STORE_SUCCESS)
	                        break;
                    }
                }
            } else if(msp432_synced && rcv_idx < MAX_SENSOR_DATA_RCV_SIZE*2)
                uart_buffer[rcv_idx++] = rcv_byte;
        }

        XIntc_AckIntr(intc, MSP432_UART_INT); // acknowledge interrupt 
    }
    else { // should not make it here
        xil_printf("WARNING: Unrecognized interrupt!\n");
    }
}

int init_board_info(u32 feature_rom) {

	write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val & ~FEATURE_ROM_ERROR);

	if(USE_FIXED_BOARD == 1) {
		xil_printf("USING FIXED BOARD INFO FOR: %s\n", FIXED_BOARD);
		memcpy(_board_info.vbnv_info, (void *)FIXED_BOARD, MAX_BOARD_INFO_LENGTH);
		_board_info.vendor = _board_info.vbnv_info;
		_board_info.board = strstr(_board_info.vendor, VBNV_SEPARATOR); _board_info.board[0] = '\0'; _board_info.board += 1;
		_board_info.name = strstr(_board_info.board, VBNV_SEPARATOR); _board_info.name[0] = '\0'; _board_info.name += 1;
		_board_info.version = strstr(_board_info.name, VBNV_SEPARATOR); _board_info.version[0] = '\0'; _board_info.version += 1;
	} else {
		memcpy(_board_info.vbnv_info, (void *)(feature_rom+VBNV_OFFSET), MAX_BOARD_INFO_LENGTH);
		xil_printf("Board VBNV: %s\n", &_board_info.vbnv_info[VBNV_OFFSET%4]);

		//Separate fields which allows for different configurations based on each field
		_board_info.vendor = &_board_info.vbnv_info[VBNV_OFFSET%4];
		_board_info.board = strstr(_board_info.vendor, VBNV_SEPARATOR); _board_info.board[0] = '\0'; _board_info.board += 1;
		_board_info.name = strstr(_board_info.board, VBNV_SEPARATOR); _board_info.name[0] = '\0'; _board_info.name += 1;
		_board_info.version = strstr(_board_info.name, VBNV_SEPARATOR); _board_info.version[0] = '\0'; _board_info.version += 1;

		xil_printf("Board vendor: %s\n", 		_board_info.vendor);
		xil_printf("Board board id: %s\n", 	_board_info.board);
		xil_printf("Board name: %s\n", 		_board_info.name);
		xil_printf("Board version: %s\n", 	_board_info.version);
	}

	if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"xil-accel-rd-vu9p-hp") == 0) {
		xil_printf("VU9P-HP\n");
		//Set board info properties
		_board_info.supplies = VU9P_HP_SUPPLIES;
		_board_info.num_supplies = VU9P_HP_NUM_SUPPLIES;
		_board_info.iic_mux_addr = VU9P_HP_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | MGTAVTT_AVAILABLE | MGTAVCCC_AVAILABLE |
		   VCCBRAM_AVAILABLE | VCC1V2_AVAILABLE | VCC1V8_AVAILABLE | VCCINT_AVAILABLE);
	} else if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"xil-accel-rd-vu9p") == 0) {
		xil_printf("VU9P\n");
		//Set board info properties
		_board_info.supplies = VU9P_SUPPLIES;
		_board_info.num_supplies = VU9P_NUM_SUPPLIES;
		_board_info.iic_mux_addr = VU9P_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | MGTAVTT_AVAILABLE | MGTAVCCC_AVAILABLE |
		    VCC1V2_AVAILABLE | VCC1V8_AVAILABLE | VCCINT_AVAILABLE);
	} else if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"xil-accel-rd-ku115") == 0) {
		xil_printf("KU115\n");
		//Set board info properties
		_board_info.supplies = KU115_SUPPLIES;
		_board_info.num_supplies = KU115_NUM_SUPPLIES;
		_board_info.iic_mux_addr = KU115_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | MGTAVTT_AVAILABLE | MGTAVCCC_AVAILABLE |
		    VCC1V2_AVAILABLE | VCC1V8_AVAILABLE | VCCINT_AVAILABLE);
	} else if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"kcu1500") == 0) {
		xil_printf("KCU1500\n");
		//Set board info properties
		_board_info.supplies = KCU1500_SUPPLIES;
		_board_info.num_supplies = KCU1500_NUM_SUPPLIES;
		_board_info.iic_mux_addr = KCU1500_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | MGTAVTT_AVAILABLE | MGTAVCCC_AVAILABLE |
		    VCC1V2_AVAILABLE | VCC1V8_AVAILABLE | VCCINT_AVAILABLE);
	} else if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"vcu1525") == 0) {
		xil_printf("VCU1525\n");
		//Set board info properties
		msp432_support = true;
		_board_info.supplies = VCU1525_SUPPLIES;
		_board_info.num_supplies = VCU1525_NUM_SUPPLIES;
		//_board_info.iic_mux_addr = VCU1525_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | BMC_COMM_SUPPORT | PEX12V_AVAILABLE |
		    AUX12V_AVAILABLE | PEX3V3_AVAILABLE | VCCINT_AVAILABLE);
	} else if(strcmp(_board_info.vendor,"xilinx") == 0 && strcmp(_board_info.board,"vcu1526") == 0) {
		xil_printf("VCU1526\n");
		//Set board info properties
		_board_info.supplies = VCU1526_SUPPLIES;
		_board_info.num_supplies = VCU1526_NUM_SUPPLIES;
		_board_info.iic_mux_addr = VCU1526_IIC_MUX_ADDR;
		//Update register map
		write_reg(&RegisterMap[FEATURES_REG], POWMON_SUPPORT | VCCINT_AVAILABLE);
	} else {
		xil_printf("ERROR: Unrecognized vbnv! %s:%s:%s:%s\n", _board_info.vendor, _board_info.board, _board_info.name, _board_info.version);
		write_reg(&RegisterMap[ERROR_REG], RegisterMap[ERROR_REG].reg_val | FEATURE_ROM_ERROR);

		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

int main ()
{
	u32 paused_time = 0;
	u32 supported_interrupts = 0;
	num_samps = 0;
	msp432_synced = false;

	//Device addresses found in xparameters.h
	u32 IIC; //Set to -1 if not supported
	u32 feature_rom = XPAR_STATIC_REGION_FEATURE_ROM_CTRL_S_AXI_BASEADDR;
	bram = XPAR_STATIC_REGION_BRD_MGMT_SCHEDULER_BOARD_MANAGEMENT_REGISTER_MAP_CTRL_S_AXI_BASEADDR;
	intc = XPAR_STATIC_REGION_BRD_MGMT_SCHEDULER_BOARD_MANAGEMENT_AXI_INTC_0_BASEADDR;
	uart = XPAR_STATIC_REGION_BRD_MGMT_SCHEDULER_BOARD_MANAGEMENT_AXI_UARTLITE_0_BASEADDR;

	//Initialize registers
	xil_printf("Initializing registers\n");
	write_reg(&RegisterMap[VERSION_REG], RegisterMap[VERSION_REG].reg_val);
	write_reg(&RegisterMap[ID_REG], RegisterMap[ID_REG].reg_val);
	for(u8 i = 2; i < NUM_REGISTERS; i++)
	    write_reg(&RegisterMap[i], 0);

	//Get board info
	if(init_board_info(feature_rom) != 0) {
		xil_printf("Failed to initialize board! Exiting!\n");
		write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | MB_STOPPED);
		return -1;
	}
	
	//Clear any stored data
	for(int i=0;i<_board_info.num_supplies;i++)
	    _board_info.supplies[i].sum_iout = 0;

	//Initialize I2C
	if(!msp432_support) {
	    IIC = XPAR_STATIC_REGION_BRD_MGMT_SCHEDULER_BOARD_MANAGEMENT_BOARD_I2C_CTRL_BASEADDR;
	    XIic_DynInit(IIC);
	    if(iic_mux_select (IIC, PMBUS_SEL) != XST_SUCCESS) {
	        xil_printf("Failed to set iic mux! Exiting!\n");
	        write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | MB_STOPPED);
	        return -1;
	    }
	} else
	    IIC = -1;

	//Initialize UART
	if(msp432_support) {
	    supported_interrupts |= MSP432_UART_INT;
	    XUartLite_SetControlReg(uart, XUL_CR_FIFO_TX_RESET | XUL_CR_FIFO_RX_RESET); //Reset fifos
	    XUartLite_EnableIntr(uart);
    }
    
    //Enable supported interrutpts
    XIntc_EnableIntr(intc, supported_interrupts);
    XIntc_MasterEnable(intc);
    microblaze_enable_interrupts();

	write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | INIT_SUCCESS);
	
	while(1){
	    u32 control_reg = read_reg(&RegisterMap[CONTROL_REG]);

	    //User requested the microblaze to pause so wait for up to 10s
	    if(control_reg & PAUSE_MB) {
	        xil_printf("Paused...\n");
	        paused_time=0;
	        write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | MB_PAUSED);
	        while(paused_time < MB_PAUSED_TIMEOUT_US && (control_reg & PAUSE_MB)) {
				usleep(10000);
				paused_time+=10000;
				control_reg = read_reg(&RegisterMap[CONTROL_REG]);
			}
			write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val & ~MB_PAUSED);
			write_reg(&RegisterMap[CONTROL_REG], RegisterMap[CONTROL_REG].reg_val & ~PAUSE_MB);
	    }

	    //User requested to clear error flags
	    if(control_reg & RESET_ERROR_FLAGS) {
	        write_reg(&RegisterMap[ERROR_REG], 0);
	        write_reg(&RegisterMap[CONTROL_REG], RegisterMap[CONTROL_REG].reg_val & ~RESET_ERROR_FLAGS);
	    }

	    //User requested to clear current readings
	    if(control_reg & RESET_CUR_READINGS) {
	        XIntc_MasterDisable(intc);
	        for(int i=0;i<_board_info.num_supplies;i++) {
			    write_reg(_board_info.supplies[i].cur_iout_reg, 0x0);
			    write_reg(_board_info.supplies[i].max_iout_reg, 0x0);
			    write_reg(_board_info.supplies[i].avg_iout_reg, 0x0);
			    _board_info.supplies[i].sum_iout = 0;
		    }
		    num_samps = 0;
	        write_reg(&RegisterMap[CONTROL_REG], RegisterMap[CONTROL_REG].reg_val & ~RESET_CUR_READINGS);
	        XIntc_MasterEnable(intc);
	    }

	    //User requested to stop microblaze
	    if(control_reg & STOP_MB)
	        if(read_reg(&RegisterMap[STOP_MB_CONFIRM_REG]) & STOP_MB_CONFIRM)
	            break;

	    //TODO: Check if request to update MMCMs
	    
	    if(!msp432_support) {
            //Read current measurements
		    num_samps+=1;
		    store_flag = STORE_SUCCESS;
		    for(int i=0;i<_board_info.num_supplies;i++) {
			    //pmbus_print_voltage(IIC, &_board_info.supplies[i]);
			    //pmbus_print_current(IIC, &_board_info.supplies[i]);
			    store_flag = store_current(IIC, &_board_info.supplies[i]);
			    if(store_flag != STORE_SUCCESS)
				    break;
		    }
	    }
		
	    //If one of the supplies overflows for averaging we reset all values
	    XIntc_MasterDisable(intc);
	    if(store_flag != STORE_SUCCESS) {
		    if(store_flag == STORE_OVERFLOW) {
			    num_samps=1;
			    store_flag = STORE_SUCCESS;

			    usleep(1000); //Pause before resetting stats. I've noticed without this it can cause I2C read errors
			    for(int i=0;i<_board_info.num_supplies;i++)
				    store_flag |= reset_average_value(IIC, &_board_info.supplies[i]);

			    if(store_flag != STORE_SUCCESS) { //Clear checksum if pmbus fails
				    write_reg(&RegisterMap[CUR_CHKSUM_REG], 0x0);
			    } else
				    write_cur_checksum();
		    }
		    else { //Clear checksum if pmbus fails and reset iic bus
			    write_reg(&RegisterMap[CUR_CHKSUM_REG], 0x0);
		    }
        } else
		    write_cur_checksum();
	    XIntc_MasterEnable(intc);
	}

    xil_printf("Microblaze stopped!\n");
    write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | MB_STOPPED);
    XIntc_MasterDisable(intc);
	return 0;
}
