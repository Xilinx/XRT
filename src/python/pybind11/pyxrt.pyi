# SPDX-License-Identifier: MIT
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
from __future__ import annotations
import sys
from enum import IntEnum
from typing import (
    Any,
    Callable,
    Iterator,
    List,
    Sequence,
    SupportsInt,
    TYPE_CHECKING,
    Union,
    overload,
)

if TYPE_CHECKING:
    import numpy as np
    import numpy.typing as npt
    NDArrayInt8 = npt.NDArray[np.int8]
else:
    try:
        import numpy as np
        import numpy.typing as npt
        NDArrayInt8 = npt.NDArray[np.int8]
    except ImportError:
        np = None  # type: ignore[assignment]
        npt = None  # type: ignore[assignment]
        NDArrayInt8 = Any

# Buffer protocol type - accepts bytes, bytearray, memoryview, numpy arrays, etc.
if sys.version_info >= (3, 12):
    from collections.abc import Buffer as ReadableBuffer
elif npt is not None:
    # For Python < 3.12, include numpy arrays when numpy is available
    ReadableBuffer = Union[bytes, bytearray, memoryview, NDArrayInt8]  # type: ignore[misc]
else:
    # Fallback without numpy
    ReadableBuffer = Union[bytes, bytearray, memoryview, Any]  # type: ignore[misc]

# Type aliases
memory_group = int
export_handle = int
pid_type = int


# Constants
XCL_BO_FLAGS_NONE: int


# =============================================================================
# Enumerations
# =============================================================================

class xclBOSyncDirection(IntEnum):
    """DMA flags used with DMA API"""
    XCL_BO_SYNC_BO_TO_DEVICE: xclBOSyncDirection
    XCL_BO_SYNC_BO_FROM_DEVICE: xclBOSyncDirection
    XCL_BO_SYNC_BO_GMIO_TO_AIE: xclBOSyncDirection
    XCL_BO_SYNC_BO_AIE_TO_GMIO: xclBOSyncDirection


class ert_cmd_state(IntEnum):
    """Kernel execution status"""
    ERT_CMD_STATE_NEW: ert_cmd_state
    ERT_CMD_STATE_QUEUED: ert_cmd_state
    ERT_CMD_STATE_COMPLETED: ert_cmd_state
    ERT_CMD_STATE_ERROR: ert_cmd_state
    ERT_CMD_STATE_ABORT: ert_cmd_state
    ERT_CMD_STATE_SUBMITTED: ert_cmd_state
    ERT_CMD_STATE_TIMEOUT: ert_cmd_state
    ERT_CMD_STATE_NORESPONSE: ert_cmd_state
    ERT_CMD_STATE_SKERROR: ert_cmd_state
    ERT_CMD_STATE_SKCRASHED: ert_cmd_state


class xrt_info_device(IntEnum):
    """Device feature and sensor information"""
    bdf: xrt_info_device
    interface_uuid: xrt_info_device
    kdma: xrt_info_device
    max_clock_frequency_mhz: xrt_info_device
    m2m: xrt_info_device
    name: xrt_info_device  # type: ignore[assignment]
    nodma: xrt_info_device
    offline: xrt_info_device
    electrical: xrt_info_device
    thermal: xrt_info_device
    mechanical: xrt_info_device
    memory: xrt_info_device
    platform: xrt_info_device
    pcie_info: xrt_info_device
    host: xrt_info_device
    dynamic_regions: xrt_info_device
    vmr: xrt_info_device


class xrt_msg_level(IntEnum):
    """XRT log message level"""
    emergency: xrt_msg_level
    alert: xrt_msg_level
    critical: xrt_msg_level
    error: xrt_msg_level
    warning: xrt_msg_level
    notice: xrt_msg_level
    info: xrt_msg_level
    debug: xrt_msg_level


# Exported enum values at module level
emergency: xrt_msg_level
alert: xrt_msg_level
critical: xrt_msg_level
error: xrt_msg_level
warning: xrt_msg_level
notice: xrt_msg_level
info: xrt_msg_level
debug: xrt_msg_level


# =============================================================================
# Global Functions
# =============================================================================

def enumerate_devices() -> int:
    """Enumerate devices in system.
    
    Returns:
        Number of devices found in the system.
    """
    ...


def log_message(level: xrt_msg_level, tag: str, message: str) -> None:
    """Dispatch formatted log message.
    
    Args:
        level: The log level for the message.
        tag: A tag string to identify the message source.
        message: The log message to dispatch.
    """
    ...


# =============================================================================
# Classes
# =============================================================================

class uuid:
    """XRT UUID object to identify a compiled xclbin binary."""
    
    def __init__(self, uuid_str: str) -> None:
        """Create a UUID from a string representation.
        
        Args:
            uuid_str: String representation of the UUID.
        """
        ...
    
    def to_string(self) -> str:
        """Convert XRT UUID object to string.
        
        Returns:
            String representation of the UUID.
        """
        ...


class hw_context:
    """A hardware context associates an xclbin with hardware resources."""
    
    @overload
    def __init__(self) -> None:
        """Create an empty hardware context."""
        ...
    
    @overload
    def __init__(self, device: device, uuid: uuid) -> None:
        """Create a hardware context for a device with a specific xclbin UUID.
        
        Args:
            device: The device to create the context on.
            uuid: The UUID of the xclbin to associate with this context.
        """
        ...
    
    @overload
    def __init__(self, device: device, elf: elf) -> None:
        """Create a hardware context for a device with an ELF object.
        
        Args:
            device: The device to create the context on.
            elf: The ELF object to associate with this context.
        """
        ...


class device:
    """Abstraction of an acceleration device."""
    
    @overload
    def __init__(self) -> None:
        """Create an empty device object."""
        ...
    
    @overload
    def __init__(self, index: SupportsInt) -> None:
        """Open a device by index.
        
        Args:
            index: Device index (0-based).
        """
        ...
    
    @overload
    def __init__(self, bdf: str) -> None:
        """Open a device by BDF string.
        
        Args:
            bdf: Device BDF (Bus:Device.Function) string.
        """
        ...
    
    @overload
    def load_xclbin(self, xclbin_path: str) -> uuid:
        """Load an xclbin given the path to the device.
        
        Args:
            xclbin_path: Path to the xclbin file.
            
        Returns:
            UUID of the loaded xclbin.
        """
        ...
    
    @overload
    def load_xclbin(self, xclbin: xclbin) -> uuid:
        """Load the xclbin to the device.
        
        Args:
            xclbin: The xclbin object to load.
            
        Returns:
            UUID of the loaded xclbin.
        """
        ...
    
    def register_xclbin(self, xclbin: xclbin) -> uuid:
        """Register an xclbin with the device.
        
        Args:
            xclbin: The xclbin object to register.
            
        Returns:
            UUID of the registered xclbin.
        """
        ...
    
    def get_xclbin_uuid(self) -> uuid:
        """Return the UUID object representing the xclbin loaded on the device.
        
        Returns:
            UUID of the currently loaded xclbin.
        """
        ...
    
    def get_info(self, key: xrt_info_device) -> str:
        """Obtain the device properties and sensor information.
        
        Args:
            key: The type of device information to retrieve.
            
        Returns:
            String representation of the requested device information.
        """
        ...


class run:
    """Represents one execution of a kernel."""
    
    @overload
    def __init__(self) -> None:
        """Create an empty run object."""
        ...
    
    @overload
    def __init__(self, kernel: kernel) -> None:
        """Create a run object from a kernel.
        
        Args:
            kernel: The kernel to create a run for.
        """
        ...
    
    def start(self) -> None:
        """Start one execution of a run."""
        ...
    
    @overload
    def set_arg(self, index: SupportsInt, bo: bo) -> None:
        """Set a specific kernel global argument for a run.
        
        Args:
            index: Argument index.
            bo: Buffer object argument.
        """
        ...
    
    @overload
    def set_arg(self, index: SupportsInt, value: SupportsInt) -> None:
        """Set a specific kernel scalar argument for this run.
        
        Args:
            index: Argument index.
            value: Integer scalar argument.
        """
        ...
    
    @overload
    def wait(self) -> ert_cmd_state:
        """Wait for the run to complete.
        
        Returns:
            Final execution state.
        """
        ...
    
    @overload
    def wait(self, timeout_ms: SupportsInt) -> ert_cmd_state:
        """Wait for the specified milliseconds for the run to complete.
        
        Args:
            timeout_ms: Timeout in milliseconds.
            
        Returns:
            Execution state when wait completes or times out.
        """
        ...
    
    @overload
    def wait2(self) -> None:
        """Wait for the run to complete."""
        ...
    
    @overload
    def wait2(self, timeout: int) -> ert_cmd_state:
        """Wait for the specified milliseconds for the run to complete.
        
        Args:
            timeout: Timeout in milliseconds.
            
        Returns:
            Execution state when wait completes or times out.
        """
        ...
    
    def state(self) -> ert_cmd_state:
        """Check the current state of a run object.
        
        Returns:
            Current execution state.
        """
        ...
    
    def add_callback(
        self,
        state: ert_cmd_state,
        callback: Callable[[Any, ert_cmd_state, Any], None],
        data: Any
    ) -> None:
        """Add a callback function for run state.
        
        Args:
            state: The state to trigger the callback on.
            callback: The callback function.
            data: User data to pass to the callback.
        """
        ...


class kernel:
    """Represents a set of instances matching a specified name."""
    
    class cu_access_mode(IntEnum):
        """Compute unit access mode."""
        exclusive: kernel.cu_access_mode
        shared: kernel.cu_access_mode
        none: kernel.cu_access_mode
    
    # Class-level enum value aliases
    exclusive: cu_access_mode
    shared: cu_access_mode
    none: cu_access_mode
    
    @overload
    def __init__(
        self,
        device: device,
        uuid: uuid,
        name: str,
        mode: cu_access_mode
    ) -> None:
        """Create a kernel with specified access mode.
        
        Args:
            device: The device to create the kernel on.
            uuid: UUID of the xclbin containing the kernel.
            name: Name of the kernel.
            mode: Access mode for compute units.
        """
        ...
    
    @overload
    def __init__(self, device: device, uuid: uuid, name: str) -> None:
        """Create a kernel with default access mode.
        
        Args:
            device: The device to create the kernel on.
            uuid: UUID of the xclbin containing the kernel.
            name: Name of the kernel.
        """
        ...
    
    @overload
    def __init__(self, ctx: hw_context, name: str) -> None:
        """Create a kernel from a hardware context.
        
        Args:
            ctx: The hardware context.
            name: Name of the kernel.
        """
        ...
    
    def __call__(self, *args: Union[bo, int]) -> run:
        """Execute the kernel with the given arguments.
        
        Args:
            *args: Kernel arguments (buffer objects or integers).
            
        Returns:
            A run object representing the kernel execution.
        """
        ...
    
    def group_id(self, index: SupportsInt) -> int:
        """Get the memory bank group id of a kernel argument.
        
        Args:
            index: Argument index.
            
        Returns:
            Memory bank group ID.
        """
        ...


class bo:
    """Represents a buffer object."""
    
    class flags(IntEnum):
        """Buffer object creation flags."""
        normal: bo.flags
        cacheable: bo.flags
        device_only: bo.flags
        host_only: bo.flags
        p2p: bo.flags
        svm: bo.flags
    
    # Class-level enum value aliases
    normal: flags
    cacheable: flags
    device_only: flags
    host_only: flags
    p2p: flags
    svm: flags
    
    @overload
    def __init__(
        self,
        device: device,
        size: SupportsInt,
        flags: flags,
        group: SupportsInt
    ) -> None:
        """Create a buffer object with specified properties.
        
        Args:
            device: The device to allocate the buffer on.
            size: Size of the buffer in bytes.
            flags: Buffer creation flags.
            group: Memory bank group ID.
        """
        ...
    
    @overload
    def __init__(self, parent: bo, size: SupportsInt, offset: SupportsInt) -> None:
        """Create a sub-buffer of an existing buffer object.
        
        Args:
            parent: Parent buffer object.
            size: Size of the sub-buffer in bytes.
            offset: Offset in the parent buffer.
        """
        ...
    
    def write(self, data: ReadableBuffer, seek: SupportsInt) -> None:
        """Write the provided data into the buffer object.
        
        Args:
            data: Data to write (any buffer protocol object: bytes, bytearray, memoryview, numpy array, etc.).
            seek: Offset in the buffer to start writing at.
        """
        ...
    
    def read(self, size: SupportsInt, skip: SupportsInt) -> NDArrayInt8:
        """Read from the buffer object.
        
        Args:
            size: Number of bytes to read.
            skip: Offset in the buffer to start reading from.
            
        Returns:
            NumPy array containing the read data.
        """
        ...
    
    @overload
    def sync(self, direction: xclBOSyncDirection, size: SupportsInt, offset: SupportsInt) -> None:
        """Synchronize (DMA or cache flush/invalidation) the buffer.
        
        Args:
            direction: Direction of synchronization.
            size: Number of bytes to sync.
            offset: Offset in the buffer.
        """
        ...
    
    @overload
    def sync(self, direction: xclBOSyncDirection) -> None:
        """Sync entire buffer content in specified direction.
        
        Args:
            direction: Direction of synchronization.
        """
        ...
    
    def map(self) -> memoryview:
        """Create a byte accessible memory view of the buffer object.
        
        Returns:
            Memory view of the buffer.
        """
        ...
    
    def size(self) -> int:
        """Return the size of the buffer object.
        
        Returns:
            Size in bytes.
        """
        ...
    
    def address(self) -> int:
        """Return the device physical address of the buffer object.
        
        Returns:
            Physical device address.
        """
        ...


class xclbin:
    """Represents an xclbin and provides APIs to access meta data."""
    
    class xclbinip:
        """Represents an IP in an xclbin."""
        
        def __init__(self) -> None:
            """Create an empty xclbinip object."""
            ...
        
        def get_name(self) -> str:
            """Get the IP name.
            
            Returns:
                Name of the IP.
            """
            ...
    
    class xclbinkernel:
        """Represents a kernel in an xclbin."""
        
        def __init__(self) -> None:
            """Create an empty xclbinkernel object."""
            ...
        
        def get_name(self) -> str:
            """Get kernel name.
            
            Returns:
                Name of the kernel.
            """
            ...
        
        def get_num_args(self) -> int:
            """Number of arguments.
            
            Returns:
                Number of kernel arguments.
            """
            ...
    
    class xclbinmem:
        """Represents a physical device memory bank."""
        
        def __init__(self) -> None:
            """Create an empty xclbinmem object."""
            ...
        
        def get_tag(self) -> str:
            """Get tag name.
            
            Returns:
                Tag name of the memory bank.
            """
            ...
        
        def get_base_address(self) -> int:
            """Get the base address of the memory bank.
            
            Returns:
                Base address.
            """
            ...
        
        def get_size_kb(self) -> int:
            """Get the size of the memory in KB.
            
            Returns:
                Size in kilobytes.
            """
            ...
        
        def get_used(self) -> bool:
            """Get used status of the memory.
            
            Returns:
                True if the memory bank is used.
            """
            ...
        
        def get_index(self) -> int:
            """Get the index of the memory.
            
            Returns:
                Memory bank index.
            """
            ...
    
    @overload
    def __init__(self) -> None:
        """Create an empty xclbin object."""
        ...
    
    @overload
    def __init__(self, filename: str) -> None:
        """Create an xclbin object from a file.
        
        Args:
            filename: Path to the xclbin file.
        """
        ...
    
    @overload
    def __init__(self, axlf_data: Any) -> None:
        """Create an xclbin object from axlf data.
        
        Args:
            axlf_data: Pointer to axlf data structure.
        """
        ...
    
    def get_kernels(self) -> List[xclbinkernel]:
        """Get list of kernels from xclbin.
        
        Returns:
            List of kernel objects.
        """
        ...
    
    def get_xsa_name(self) -> str:
        """Get Xilinx Support Archive (XSA) name of xclbin.
        
        Returns:
            XSA name string.
        """
        ...
    
    def get_uuid(self) -> uuid:
        """Get the uuid of the xclbin.
        
        Returns:
            UUID object.
        """
        ...
    
    def get_mems(self) -> List[xclbinmem]:
        """Get list of memory objects.
        
        Returns:
            List of memory bank objects.
        """
        ...
    
    def get_axlf(self) -> Any:
        """Get the axlf data of the xclbin.
        
        Returns:
            Pointer to axlf data structure.
        """
        ...


class xclbinip_vector(Sequence[xclbin.xclbinip]):
    """Vector of xclbin IP objects."""
    
    def __init__(self) -> None: ...
    def __len__(self) -> int: ...
    def __getitem__(self, index: int) -> xclbin.xclbinip: ...  # type: ignore[override]
    def __setitem__(self, index: int, value: xclbin.xclbinip) -> None: ...
    def __delitem__(self, index: int) -> None: ...
    def __iter__(self) -> Iterator[xclbin.xclbinip]: ...
    def __bool__(self) -> bool: ...
    def append(self, value: xclbin.xclbinip) -> None: ...
    def clear(self) -> None: ...
    def extend(self, values: Sequence[xclbin.xclbinip]) -> None: ...
    def insert(self, index: int, value: xclbin.xclbinip) -> None: ...
    def pop(self, index: int = -1) -> xclbin.xclbinip: ...


class xclbinkernel_vector(Sequence[xclbin.xclbinkernel]):
    """Vector of xclbin kernel objects."""
    
    def __init__(self) -> None: ...
    def __len__(self) -> int: ...
    def __getitem__(self, index: int) -> xclbin.xclbinkernel: ...  # type: ignore[override]
    def __setitem__(self, index: int, value: xclbin.xclbinkernel) -> None: ...
    def __delitem__(self, index: int) -> None: ...
    def __iter__(self) -> Iterator[xclbin.xclbinkernel]: ...
    def __bool__(self) -> bool: ...
    def append(self, value: xclbin.xclbinkernel) -> None: ...
    def clear(self) -> None: ...
    def extend(self, values: Sequence[xclbin.xclbinkernel]) -> None: ...
    def insert(self, index: int, value: xclbin.xclbinkernel) -> None: ...
    def pop(self, index: int = -1) -> xclbin.xclbinkernel: ...


class xclbinmem_vector(Sequence[xclbin.xclbinmem]):
    """Vector of xclbin memory objects."""
    
    def __init__(self) -> None: ...
    def __len__(self) -> int: ...
    def __getitem__(self, index: int) -> xclbin.xclbinmem: ...  # type: ignore[override]
    def __setitem__(self, index: int, value: xclbin.xclbinmem) -> None: ...
    def __delitem__(self, index: int) -> None: ...
    def __iter__(self) -> Iterator[xclbin.xclbinmem]: ...
    def __bool__(self) -> bool: ...
    def append(self, value: xclbin.xclbinmem) -> None: ...
    def clear(self) -> None: ...
    def extend(self, values: Sequence[xclbin.xclbinmem]) -> None: ...
    def insert(self, index: int, value: xclbin.xclbinmem) -> None: ...
    def pop(self, index: int = -1) -> xclbin.xclbinmem: ...


class elf:
    """ELF representation of compiled AIE binary."""
    
    @overload
    def __init__(self, filename: str) -> None:
        """Create an ELF object from a file.
        
        Args:
            filename: Path to the ELF file.
        """
        ...
    
    @overload
    def __init__(self, data: Any, size: SupportsInt) -> None:
        """Create an ELF object from memory.
        
        Args:
            data: Pointer to ELF data in memory.
            size: Size of the ELF data.
        """
        ...


class program:
    """Represents a compiled program to be executed on the AIE.
    
    The program is an ELF file with sections and data specific to the AIE.
    """
    
    def __init__(self, elf: elf) -> None:
        """Create a program from an ELF object.
        
        Args:
            elf: The ELF object containing the program.
        """
        ...
    
    def get_partition_size(self) -> int:
        """Required partition size to run the program.
        
        Returns:
            Required partition size.
        """
        ...


class module:
    """Functions an application will execute in hardware."""
    
    def __init__(self, elf: elf) -> None:
        """Create a module from an ELF object.
        
        Args:
            elf: The ELF object containing the module.
        """
        ...
    
    def get_hw_context(self) -> hw_context:
        """Get hw context of module.
        
        Returns:
            Hardware context associated with the module.
        """
        ...


class runlist:
    """Represents a list of runs to be executed."""
    
    @overload
    def __init__(self) -> None:
        """Create an empty runlist."""
        ...
    
    @overload
    def __init__(self, hwctx: hw_context) -> None:
        """Create a runlist with a hardware context.
        
        Args:
            hwctx: Hardware context for the runlist.
        """
        ...
    
    def add(self, run: run) -> None:
        """Add a run to the runlist.
        
        Args:
            run: Run object to add.
        """
        ...
    
    def execute(self) -> None:
        """Execute all runs in the runlist."""
        ...
    
    @overload
    def wait(self) -> None:
        """Wait for all runs in the runlist to complete."""
        ...
    
    @overload
    def wait(self, timeout: SupportsInt) -> ert_cmd_state:
        """Wait for the specified timeout for the runlist to complete.
        
        Args:
            timeout: Timeout in milliseconds.
            
        Returns:
            Execution state when wait completes or times out.
        """
        ...


# =============================================================================
# ext submodule - Extended functionality
# =============================================================================

# The ext module is a submodule, define its contents as a namespace
class _ext_access_mode(IntEnum):
    """External buffer access mode."""
    none: _ext_access_mode
    read: _ext_access_mode
    write: _ext_access_mode
    read_write: _ext_access_mode
    local: _ext_access_mode
    shared: _ext_access_mode
    process: _ext_access_mode
    hybrid: _ext_access_mode
    
    def __or__(self, other: _ext_access_mode) -> _ext_access_mode: ...  # type: ignore[override]
    def __and__(self, other: _ext_access_mode) -> _ext_access_mode: ...  # type: ignore[override]
    def __ior__(self, other: _ext_access_mode) -> _ext_access_mode: ...
    def __iand__(self, other: _ext_access_mode) -> _ext_access_mode: ...


class _ext_bo(bo):
    """Represents an enhanced version of xrt::bo with support for access mode."""
    
    @overload
    def __init__(
        self, device: device, userptr: Any, size: SupportsInt, access: _ext_access_mode
    ) -> None:
        """Create a buffer with user pointer and access mode.
        
        Args:
            device: The device.
            userptr: User pointer to memory.
            size: Size in bytes.
            access: Access mode.
        """
        ...
    
    @overload
    def __init__(self, device: device, userptr: Any, size: SupportsInt) -> None:
        """Create a buffer with user pointer.
        
        Args:
            device: The device.
            userptr: User pointer to memory.
            size: Size in bytes.
        """
        ...
    
    @overload
    def __init__(
        self, device: device, size: SupportsInt, access: _ext_access_mode
    ) -> None:
        """Create a buffer with access mode.
        
        Args:
            device: The device.
            size: Size in bytes.
            access: Access mode.
        """
        ...
    
    @overload
    def __init__(self, device: device, size: SupportsInt) -> None:
        """Create a buffer on device.
        
        Args:
            device: The device.
            size: Size in bytes.
        """
        ...
    
    @overload
    def __init__(
        self, device: device, pid: pid_type, export_handle: SupportsInt
    ) -> None:
        """Create a buffer from exported handle.
        
        Args:
            device: The device.
            pid: Process ID.
            export_handle: Export handle.
        """
        ...
    
    @overload
    def __init__(
        self, hwctx: hw_context, size: SupportsInt, access: _ext_access_mode
    ) -> None:
        """Create a buffer with hardware context and access mode.
        
        Args:
            hwctx: Hardware context.
            size: Size in bytes.
            access: Access mode.
        """
        ...
    
    @overload
    def __init__(self, hwctx: hw_context, size: SupportsInt) -> None:
        """Create a buffer with hardware context.
        
        Args:
            hwctx: Hardware context.
            size: Size in bytes.
        """
        ...
    
    @overload
    def __init__(
        self, hwctx: hw_context, pid: pid_type, export_handle: SupportsInt
    ) -> None:
        """Create a buffer from exported handle with hardware context.
        
        Args:
            hwctx: Hardware context.
            pid: Process ID.
            export_handle: Export handle.
        """
        ...


class _ext_kernel(kernel):
    """Represents an external kernel object."""
    
    @overload
    def __init__(self, ctx: hw_context, mod: module, name: str) -> None:
        """Create an external kernel from module.
        
        Args:
            ctx: Hardware context.
            mod: Module containing the kernel.
            name: Kernel name.
        """
        ...
    
    @overload
    def __init__(self, ctx: hw_context, name: str) -> None:
        """Create an external kernel.
        
        Args:
            ctx: Hardware context.
            name: Kernel name.
        """
        ...


class ext:
    """Submodule for extended XRT functionality."""
    
    # Type aliases for the ext submodule
    access_mode = _ext_access_mode
    bo = _ext_bo
    kernel = _ext_kernel
    
    # Exported enum values at module level
    none: _ext_access_mode
    read: _ext_access_mode
    write: _ext_access_mode
    read_write: _ext_access_mode
    local: _ext_access_mode
    shared: _ext_access_mode
    process: _ext_access_mode
    hybrid: _ext_access_mode
