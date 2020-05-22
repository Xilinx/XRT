XMA 19.2 Migration
==================

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

In **19.1**, the buffer creation process was complex. It was needed to specify all buffer properties such as buffer size, physical address, handle when creating the buffer.

In **19.2**, the buffer creation process is simplified

   - A new type ``XmaBufferObj`` is introduced
   - Function ``xma_plg_buffer_alloc`` is changed

       - Works on ``XMASession`` directly
       - Supports Device Only Buffer  
       - Now there is no need to pass physical address using ``xma_plg_get_paddr``

Buffer Write: ``xma_plg_buffer_write``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In **19.1**, the API ``xma_plg_buffer_write`` was used to copy data from the host pointer followed by the DMA transfer. 

In **19.2**, the API ``xma_plg_buffer_write`` does not accept host pointer any longer. If desired, the user has to copy from the host pointer to the buffer. **The API now only performs DMA transfer from host to device**. 

Buffer Read: ``xma_plg_buffer_read`` 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In **19.1**, the API ``xma_plg_buffer_read.`` used to copy data to the host pointer after DMA transfer.

In **19.2**, the API ``xma_plg_buffer_read`` does not accept host pointer any longer. If desired, the user has to copy from the buffer to the host pointer. **The API now only performs DMA transfer from device to host**.             
 
Scalar/Register Write: ``xma_plg_register_prep_write``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In **19.2**, the API ``xma_plg_register_prep_write`` works directly on ``XMASession`` instead of ``XmaHwSession``. This API has to be guarded between ``xma_plg_kernel_lock_regmap`` and ``xma_plg_kernel_unlock_regmap`` APIs. 

**Note**: The API ``xma_plg_register_prep_write`` is not needed if the new scheduling API ``xma_plg_schedule_work_item_with_args`` is used.   

Kernel Scheduling: ``xma_plg_schedule_work_item``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now in **19.2**, the kernel scheduling API ``xma_plg_schedule_work_item`` has to be guarded between ``xma_plg_kernel_lock_regmap`` and ``xma_plg_kernel_unlock_regmap`` APIs.  

**Note**: If using new scheduling API ``xma_plg_schedule_work_item_with_args`` the above locking and unlocking APIs are not required.

Application Code Migration
--------------------------

XMA Initialize Function: ``xma_initlialize``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In **19.1**, the API ``xma_initlialize`` used to take YAML configuration file. 

In **19.2**, the usage of **YAML file has been obsoleted**. The API ``xma_initlialize`` takes device information and XCLBIN through new datatype: ``XmaXclbinParameter``   

Expanded Plugin Properties
~~~~~~~~~~~~~~~~~~~~~~~~~~

Plugin properties have been expanded in **19.2** to contain more information.

For example, compare ``XmaEncoderProperties`` between 19.1 and 19.2 XRT versions.      

Now in **19.2**, more fields have been added.                                

     - Device index (Should be passed from the application level)                
     - Compute Unit index (Should be determined at the application level)                                      
     - Channel id (Each channel should be passed with unique id from the application level)                                                      
     - DDR bank index (Set -1 if auto-detection is desired)            
     - Plugin lib path                                                  
