/*
 * board_mon.c
 *
 *  Created on: Oct 3, 2016
 *      Author: elleryc
 */

#include <stdlib.h>
#include <stdio.h>
#include <mb_interface.h>
#include "xbram_hw.h"
#include "sleep.h"

#define ERT_UNUSED __attribute__((unused))

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

//Register definitions
#define NUM_REGISTERS 7
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
//Feature support Register
#define FEATURES_REG 4
#define FEATURES_REG_ADDR           0x0010
//Reset and control register
#define CONTROL_REG 5
#define CONTROL_REG_ADDR            0x0018
#define STOP_MB                     0x00000008 //Request to stop microblaze. Must set confirmation bit
//Stop confirmation register
#define STOP_MB_CONFIRM_REG 6
#define STOP_MB_CONFIRM_REG_ADDR    0x001C
#define STOP_MB_CONFIRM             0x00000001 //Must be set for microblaze to stop

// Register struct
typedef struct {
    u16 addr;
    u32 reg_val;
} Register;

//Register Map
Register RegisterMap[NUM_REGISTERS] = {
    {.addr = VERSION_REG_ADDR,          .reg_val = BOARD_MON_VERSION_NUM},
    {.addr = ID_REG_ADDR,               .reg_val = ID_STRING},
    {.addr = STATUS_REG_ADDR,           .reg_val = 0},
    {.addr = ERROR_REG_ADDR,            .reg_val = 0},
    {.addr = FEATURES_REG_ADDR,         .reg_val = 0},
    {.addr = CONTROL_REG_ADDR,          .reg_val = 0},    
    {.addr = STOP_MB_CONFIRM_REG_ADDR,  .reg_val = 0},
};

//Global variables
u32 bram = 0; //BRAM address offset
// function headers
void write_reg(Register *reg, u32 val);
u32 read_reg(Register *reg);
// End function headers

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


int main ()
{
	//Device addresses found in xparameters.h
	bram = XPAR_STATIC_REGION_BRD_MGMT_SCHEDULER_BOARD_MANAGEMENT_REGISTER_MAP_CTRL_S_AXI_BASEADDR;

	//Initialize registers
	xil_printf("Initializing registers\n");
	write_reg(&RegisterMap[VERSION_REG], RegisterMap[VERSION_REG].reg_val);
	write_reg(&RegisterMap[ID_REG], RegisterMap[ID_REG].reg_val);
	for(u8 i = 2; i < NUM_REGISTERS; i++)
	    write_reg(&RegisterMap[i], 0);

	write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | INIT_SUCCESS);

    while(1) {
        u32 control_reg = read_reg(&RegisterMap[CONTROL_REG]);

        //User requested to stop microblaze
        if(control_reg & STOP_MB)
            if(read_reg(&RegisterMap[STOP_MB_CONFIRM_REG]) & STOP_MB_CONFIRM)
                break;
        
        usleep(1000);
    }

    xil_printf("Microblaze stopped!\n");
    write_reg(&RegisterMap[STATUS_REG], RegisterMap[STATUS_REG].reg_val | MB_STOPPED);
	return 0;
}
