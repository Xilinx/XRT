.. _xclbintools.rst:

xclbinutil
----------

``xclbinutil`` can create, modify and report xclbin content information. 

Common Use Cases
~~~~~~~~~~~~~~~~

Get information from xclbin file
................................

Various information can be obtained through the --info option, including information on the creation date, hardware platform, clocks, memory configuration, kernel, tool generation options, and etc. Note that optionally an output file can be specified. If none is specified, then the output will go to the console.

  Getting Information from xclbin ::
       
    xclbinutil -i binary_container_1.xclbin [--info=<FILE_NAME>]

Xclbin sections controlling
...........................

This include various operations on xclbin sections, like extracting the bitstream image, extracting the build metadata, removing a section and so on.

  Extracting the bitstream image :: 
  
    xclbinutil --dump-section BITSTREAM:RAW:bitstream.bit --input binary_container_1.xclbin

  Extracting the build metadata ::
		
    xclbinutil --dump-section BUILD_METADATA:HTML:buildMetadata.json --input binary_container_1.xclbin

  Removing a section :: 

    xclbinutil --remove-section BITSTREAM --input binary_container_1.xclbin --output binary_container_modified.xclbin

Migrate xclbin file forward from older version
..............................................

This is to migrate an older xclbin binary format (via the mirror JSON data) forward to the current binary format (compatible with new xclbin.h).

  Migrating forward ::
    
    xclbinutil --migrate-forward -i binary_container_1.xclbin -o output.xclbin

For more details please refer `Vitis Application Acceleration Development Flow Documentation <https://www.xilinx.com/html_docs/xilinx2019_2/vitis_doc/Chunk1807393241.html>`_
