.. _xma_migration_label:

XMA Migration to SDAccel 2018.3/2019.1
--------------------------------------

Caution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* 2018.3: Use latest or RC4 branch of 2018.3 XRT
* Do not use older versions of libxmaplugin.so and/or host application executable (eg. FFMPEG) with newer version of XRT. Recompile all plugins, host application using appropriate XRT.
* For each DSA use officially supported XRT release. Do not use old DSA with new XRT release which doesn't officially support that DSA
* Ensure RTL Kernels are compatible with kernels as defined in SDAccel User Guide. 

This document describes some required steps to migrate the XMA framework to the SDAccel 18.3 (and beyond) version.

1. API changes
2. YAML file change
3. sdaccel.ini change
4. Kernel reset changes in 2019.1


API changes
...........

In earlier versions of XMA plugin **xma_plg_register_write** and **xma_plg_register_read** were used for various purposes. However starting from 2018.3, xma_plg_register_write and xlc_plg_register_read are deprecated and new APIs are provided at a higher level of abstraction. The new APIs are purposed-based. So instead of direct register read/write the user will use appropriate higher-level purposed based API to achieve the same result.

Towards that end, XMA now offers a new execution model with three new APIs.

The new APIs are:

**xma_plg_register_prep_write** : will take in the request for work to be done (as set of register values) 
**xma_plg_schedule_work_item** : will schedule the work item at front of the queue once previous work item finishes
**xma_plg_is_work_item_done** : will check if the current work item is completed, so that next work item can start execution

Let's consider the various purposes where the above APIs would be useful.

Purpose 1:
The API *xma_plg_register_write* was used to send scaler inputs to the kernel by directly writing to the AXI-LITE registers. Now the higher level API *xma_plg_register_prep_write* should be used for the same purpose.

Purpose 2:
The API *xma_plg_register_write* was also used to start the kernel by writing to the start bit of the AXI-LITE registers. For this purpose, the new API *xma_plg_schedule_work_item* should be used instead of xma_plg_register_write.

Purpose 3:
The API *xma_plg_register_read* was used to check kernel idle status (by reading AXI-LITE register bit) to determine if the kernel finished processing the operation. For this purpose now the new API *xma_plg_is_work_item_done* should be used.



The below table summarizes how to migrate to the new APIs from xma_plg_register_write/xma_plg_register_read.

+----------------------------------------+-------------------------+------------------------------+
| Purposes                               |  Earlier API            |  New API                     |
+========================================+=========================+==============================+
| Sending scalar input                   | xma_plg_register_write  |  xma_plg_register_prep_write |
+----------------------------------------+-------------------------+------------------------------+
| Starting the kernel                    | xma_plg_register_write  |  xma_plg_schedule_work_item  |
+----------------------------------------+-------------------------+------------------------------+
| Checking if kernel finished processing | xma_plg_register_read   | xma_plg_is_work_item_done    |
+----------------------------------------+-------------------------+------------------------------+


YAML file change
................

**DDR bank settings**

* From 2018.3 (latest or RC4) onwards banks are automatically detected by XMA/XRT from xclbin
   
* DDR settings from YAML file are not required and ignored if provided. 

**Log message settings**

Instead of YAML file, use .ini file for logfile setting. 

However, in 2018.3 usage of logfile setting through YAML file is still supported. From 2019.1, logfile settings in YAML file are ignored (strictly use .ini file for this purpose). 

Example settings::
    
    [Runtime]
    runtime_log="runtime_log.txt" 
    verbosity = 7  
     

INI file changes
................

**Multiprocess** 

Use multi-process setting when multiple processes will share a FPGA board. To do this, enable multiprocess flag in ini file::

  [Runtime]
  multiprocess=true

Note: From 19.1 release, multiprocess is ON by default, so above setting is required for 18.3 release.

**Debug settings**

Ensure ini file is present in CWD from where xcdr, xffmpeg, xbutil program, etc commands are issued.

To enable Kernel Driver Scheduler (KDS) please do the following settings inside .ini file. **Do not use any other combination of kds and ert**::  

    [Runtime]
    kds = true 
    ert = false

Kernel Reset changes in 2019.1
..............................
From 2019.1 onwards XRT will reset kernels only on first loading of xclbin. Reloading of same xclbin will not reset Kernels.

So for stateful kernel start of a new job or new channel plugin must ensure to reset kernel or channel. This will ensure that if a kernel/channel was stuck in some state then it will reinitialize to start in correct state.
