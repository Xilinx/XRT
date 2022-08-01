Migration from Legacy XMA
=========================

Summary of changes
------------------
1. `Plugin Code Migration`_
      - `Plugin Structure Change`_
      - `Buffer Creation: xma_plg_buffer_alloc`_
      - `Buffer Write: xma_plg_buffer_write`_
      - `Buffer Read: xma_plg_buffer_read`_ 
      - `Scalar/Register Write: xma_plg_register_prep_write`_
      - `Kernel Scheduling: xma_plg_schedule_work_item`_
2. `Application Code Migration`_
      - `XMA Initialize Function: xma_initlialize`_
      - `Expanded Plugin Properties`_



Plugin code Migration
---------------------

Plugin Structure Change
~~~~~~~~~~~~~~~~~~~~~~~

The following changes have been incorporated in the main plugin structures: ``XmaEncoderPlugin``, ``XmaDecoderPlugin``, etc.

  1. The ``.alloc_chan`` plugin function has been deprecated. The resource management should be done in the application level.            
  2. The field ``.kernel_data_size`` is removed.                                                   
  3. The new field ``.xma_version`` is added.     
  
Buffer Creation: ``xma_plg_buffer_alloc``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In legacy XMA, the buffer creation process was complex. It was needed to specify all buffer properties such as buffer size, physical address, handle when creating the buffer.

In current XMA, the buffer creation process is simplified

   - A new type ``XmaBufferObj`` is introduced
   - Function ``xma_plg_buffer_alloc`` is changed

       - Works on ``XMASession`` directly
       - Supports Device Only Buffer  
       - Now there is no need to pass physical address using ``xma_plg_get_paddr``

Buffer Write: ``xma_plg_buffer_write``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In legacy XMA the API ``xma_plg_buffer_write`` was used to copy data from the host pointer followed by the DMA transfer. 

In current XMA the API ``xma_plg_buffer_write`` does not accept host pointer any longer. If desired, the user has to copy from the host pointer to the buffer. **The API now only performs DMA transfer from host to device**. 

Buffer Read: ``xma_plg_buffer_read`` 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In legacy XMA, the API ``xma_plg_buffer_read.`` used to copy data to the host pointer after DMA transfer.

In current XMA, the API ``xma_plg_buffer_read`` does not accept host pointer any longer. If desired, the user has to copy from the buffer to the host pointer. **The API now only performs DMA transfer from device to host**.             
 
Scalar/Register Write: ``xma_plg_register_prep_write``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In current XMA, the API ``xma_plg_register_prep_write`` works directly on ``XMASession`` instead of ``XmaHwSession``. This API has to be guarded between ``xma_plg_kernel_lock_regmap`` and ``xma_plg_kernel_unlock_regmap`` APIs. 

**Note**: The API ``xma_plg_register_prep_write`` is not needed if the new scheduling API ``xma_plg_schedule_work_item_with_args`` is used.   

Kernel Scheduling: ``xma_plg_schedule_work_item``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the current XMA, the kernel scheduling API ``xma_plg_schedule_work_item`` has to be guarded between ``xma_plg_kernel_lock_regmap`` and ``xma_plg_kernel_unlock_regmap`` APIs.  

**Note**: If using new scheduling API ``xma_plg_schedule_work_item_with_args`` the above locking and unlocking APIs are not required.

Application Code Migration
--------------------------

XMA Initialize Function: ``xma_initlialize``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In legacy XMA, the API ``xma_initlialize`` used to take YAML configuration file. 

In current XMA, the usage of **YAML file has been obsoleted**. The API ``xma_initlialize`` takes device information and XCLBIN through new datatype: ``XmaXclbinParameter``   

Expanded Plugin Properties
~~~~~~~~~~~~~~~~~~~~~~~~~~

Plugin properties have been expanded in current XMA to contain more information.

For example, compare ``XmaEncoderProperties`` between legacy and current XRT versions.      

Now in current XMA, more fields have been added.                                

     - Device index (Should be passed from the application level)                
     - Compute Unit index (Should be determined at the application level)                                      
     - Channel id (Each channel should be passed with unique id from the application level)                                                      
     - DDR bank index (Set -1 if auto-detection is desired)            
     - Plugin lib path                                                  


List of details changes
~~~~~~~~~~~~~~~~~~~~~~~

1. YAML configuration file is not used by XMA
2. Resource management is not handled by XMA

   a. So channel_id for multi-channel kernels must be handled in host video application (like ffmpeg)
   b. channel_id is input to XMA in session_create API as part of the properties argument
   c. See XRM for resource management details

3. MPSoC PL & soft kernels are supported in XMA
4. Direct register read & write is not available
5. DataFlow kernels are supported
6. ZeroCopy support has changed. See below for details
7. BufferObject added. See below for details
8. XmaFrame & XmaDataBuffer can use device buffers instead of host only memory
9. Support for device_only buffers
10. Session creation & destroy APIs are thread safe now
11. Multi-process support is from XRT
12. schedule_work_item  API changed to return CUCmdObj
13. New API xma_plg_schedule_cu_cmd & xma_plg_cu_cmd_status can be used instead of schedule_work_item
14. In a session if using xma_plg_cu_cmd_status then do NOT use xma_plg_is_work_item_done in same session
15. Supports up to 128 CUs per device
16. CU register map size < 4KB
17. By default XMA will automatically select default ddr bank for new device buffers (as per selected CU). Session_create may provide user selected default ddr bank input when XMA will use user select default ddr bank for plugin with that session
18. For using ddr bank other than default session ddr_bank use APIs xma_plg_buffer_alloc_arg_num(). See below for info
19. XMA now support multiple ddr bank per plugin. See below for info on xma_plg_buffer_alloc_arg_num()
20. XMA version check API added to plugin struct. See below for details
21. New session type XMA_ADMIN for non-video applications to control multiple CUs in single session. See below for details
22. get_session_cmd_load(): Get CU command load of various sessions relative to each other. Printed to log file
23. CU command load of all session is automatically sent to log file at end of the application
24. This gives info on which sessions (or CUs) are more busy compared to other sessions (or CUs)
25. QDMA platform: Host to kernel streams will be supported by XMA in future. See below for more details
