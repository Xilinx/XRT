# XRT Capture and Replay

XRT provides a capture and replay mechanism that allows recording application execution and replaying it for debugging, validation, and performance analysis.

## Overview

The capture system records XRT API calls at the frame level, where a frame is defined as:
- A single execution of `xrt::run::start()`, or
- A single execution of `xrt::runlist::execute()`

The captured data includes:
- Hardware contexts (xclbins or ELF programs)
- Buffer objects and their data
- Kernel objects and control code
- Run objects with their arguments
- Frame execution sequence and synchronization points
- Application thread context for each frame

## Capturing an Application

### Method 1: Using xrt-capture (Recommended)

The `xrt-capture` utility automatically configures and launches your application with capture enabled:

```bash
% xrt-capture --frames 10 --output-dir /tmp/xrt_capture -- ./my_xrt_application [args]
```

**Options:**
- `--frames <num>` - Number of frames to capture (required)
- `--output-dir <path>` - Output directory for artifacts (default: ./xrt_capture)

**Examples:**

```bash
# Basic capture
% xrt-capture --frames 10 -- ./my_app

# Capture with custom output directory
% xrt-capture --frames 20 --output-dir /tmp/capture -- ./my_app arg1 arg2
```

**How it works:**
1. Sets environment variables for capture configuration
2. Launches your application (inherits environment)
3. Waits for completion
4. Reports capture location and replay command

The capture settings are passed via environment variables (`Runtime.capture_frames` and `Runtime.capture_output_dir`), which XRT's config reader checks before consulting `xrt.ini`. This avoids modifying any files on disk.

### Method 2: Manual Configuration via Environment Variables

Set environment variables before running your application:

```bash
% export Runtime.capture_frames=10
% export Runtime.capture_output_dir=/tmp/xrt_capture
% ./my_xrt_application
```

### Method 3: Manual Configuration via xrt.ini

Create or edit `xrt.ini` in your application's directory:

```ini
[Runtime]
# Number of frames to capture (0 = disabled)
capture_frames=10

# Directory for captured artifacts (default: current directory)
capture_output_dir=/tmp/xrt_capture
```

Then run your application:

```bash
% ./my_xrt_application
```

Note: Environment variables take precedence over `xrt.ini` settings.

### Captured Output

The capture system generates:
1. **`replay.json`** - JSON script describing the captured execution
2. **Artifact files** - Binary files containing:
   - XCLBINs
   - ELF programs (control code)
   - Buffer data snapshots

All files are written to the configured output directory.

## Replaying Captured Data

### Basic Replay

Use `xrt-replay` executable to replay captured execution:

```bash
% xrt-replay --replay /tmp/xrt_capture/replay.json --dir /tmp/xrt_capture
```

### Replay Options

```
xrt-replay [options]

Options:
  --replay <replay.json> Replay script to execute (required)
  [--dir <path>]         Directory containing artifacts (default: current directory)
  [--iter <num>]         Number of iterations to execute (default: 1)
  [--report [<file>]]    Output metrics to <file> or use stdout for no <file>

  [--help, -h]           Show help message
```

### Examples

**Replay once:**
```bash
% xrt-replay --replay replay.json --dir /tmp/xrt_capture
```

**Replay multiple iterations for performance testing:**
```bash
% xrt-replay --replay replay.json --dir /tmp/xrt_capture --iter 100
```

**Replay with artifacts in different directory:**
```bash
% xrt-replay --replay /path/to/replay.json --dir /different/path/to/artifacts
```

## Capture Internals

### Frame Boundaries

A frame represents a single execution unit:
- **Single run**: Application calls `xrt::run::start()`
- **Runlist**: Application calls `xrt::runlist::execute()`

The same `xrt::run` or `xrt::runlist` object can be used across multiple frames, but each frame captures the state at the point of execution.

### Buffer Data Capture

The capture system uses a smart invalidation mechanism:
- Buffer data is only captured when `xrt::bo::sync(XCL_BO_SYNC_BO_TO_DEVICE)` is called
- Unchanged buffers are not re-dumped across frames
- Each frame records which buffers need to be restored during replay

### Wait Synchronization

The capture system records all `xrt::run::wait()` and `xrt::runlist::wait()` calls:
- Waits are associated with the calling thread's active frame
- Replay executes waits in the same order as captured
- Handles asynchronous execution correctly
- Cross-thread waits are preserved (Thread A can wait on a frame started by Thread B)

### Multi-threaded Capture

The capture system is thread-aware:
- Each frame records the thread ID that executed `start()` or `execute()`
- Waits are attributed to the calling thread's active frame
- Multiple threads can execute frames concurrently
- Frame execution order is preserved per-thread during replay

## Replay JSON Schema

The generated `replay.json` follows this structure:

```json
{
  "version": "1.0",
  "resources": {
    "hwctxs": [
      {
        "name": "<unique_id>",
        "cfg": { "key": "value" },
        "xclbin": "<xclbin_file>"
      }
    ],
    "buffers": [
      {
        "name": "<buffer_id>",
        "size": <bytes>,
        "type": "inout"
      }
    ],
    "kernels": [
      {
        "name": "<kernel_id>",
        "instance": "<kernel_instance_name>",
        "hwctx": "<hwctx_id>",
        "ctrlcode": "<elf_file>"
      }
    ],
    "runs": [
      {
        "name": "<run_id>",
        "kernel": "<kernel_id>",
        "arguments": [
          {
            "bo": "<buffer_id>",
            "argidx": <index>,
            "type": "inout"
          }
        ],
        "constants": [
          {
            "value": <scalar_value>,
            "argidx": <index>,
            "type": "int"
          }
        ]
      }
    ]
  },
  "execution": {
    "threads": ["<thread_id_1>", "<thread_id_2>"],
    "frames": [
      {
        "name": "<frame_id>",
        "tid": "<thread_id>",
        "runs": [
          {
            "run": "<run_id>",
            "arguments": [
              {
                "bo": "<buffer_id>",
                "fnm": "<data_file>"
              }
            ]
          }
        ],
        "waits": ["<frame_id>"]
      }
    ]
  }
}
```

### Schema Components

#### Resources Section

- **hwctxs**: Hardware contexts created from xclbins or ELF programs
- **buffers**: Buffer objects used as kernel arguments
- **kernels**: Kernel instances with control code
- **runs**: Run objects with their static argument configuration

#### Execution Section

- **threads**: Array of unique thread identifiers from capture
- **frames**: Ordered list of frame executions
  - **tid**: Thread identifier that executed this frame
  - **runs**: Which run objects are executed in this frame
  - **arguments**: Which buffer data files to load for each run
  - **waits**: Which frames must complete before next frame can start

## Use Cases

### Debugging

Capture a failing application run and replay it multiple times to debug:

```bash
# Capture failure
% ./app_with_bug
% xrt-replay --replay capture/replay.json --dir capture
```

### Performance Analysis

Capture once, replay multiple times to eliminate application overhead:

```bash
% xrt-replay --replay perf/replay.json --dir perf --iter 1000
```

### Regression Testing

Capture golden run, replay and validate output:

```bash
% # Capture baseline
% ./validated_app
% mv /tmp/capture /tmp/golden

% # Test new version
% xrt-replay --replay /tmp/golden/replay.json --dir /tmp/golden
```

### Workload Characterization

Capture complex multi-layered applications (VAIML, PyTorch, etc.) and analyze:

```bash
% python ml_inference.py  # Complex software stack
% xrt-replay --replay capture/replay.json --dir capture
% # Analyze captured kernels, buffers, execution patterns
```

### Multi-threaded Application Testing

Capture and replay multi-threaded applications with preserved threading patterns:

```bash
% # Capture multi-threaded application
% xrt-capture --frames 100 -- ./multithreaded_app

% # Replay with same thread count and execution pattern
% xrt-replay --replay /tmp/xrt_capture/replay.json --dir /tmp/xrt_capture --iter 10
```

The replay will create the same number of threads as the captured application and execute frames in their original thread context.

## Limitations

### Current Limitations

1. **Device Selection**: Replay always uses device 0
2. **Buffer Types**: All buffers are marked as "inout" (no input/output classification)
3. **Verification**: Output validation is not yet implemented
4. **Timeouts**: Wait timeouts are not captured

### Capture Overhead

When enabled, capture adds:
- File I/O for buffer dumps (proportional to sync frequency)
- JSON serialization at process exit
- Memory overhead for tracking run/buffer state

When disabled (`capture_frames=0`):
- Near-zero runtime overhead (branch prediction optimization)
- No memory allocation
- No file I/O

## Troubleshooting

### Capture Not Working

Check that:
1. `capture_frames` is set to non-zero value in `xrt.ini`
2. `capture_output_dir` exists and is writable
3. Application actually executes frames (calls `start()` or `execute()`)

### Replay Fails

Common issues:
1. **Missing artifacts**: Ensure `--dir` points to directory with binary files
2. **Device not found**: Check that device 0 is available and programmed
3. **Memory errors**: Large buffers may exceed device memory

### Debug Output

Enable XRT debug messages:

```bash
% export XRT_VERBOSITY=6
% xrt-replay --replay replay.json --dir capture
```

## Thread-Aware Replay

### Overview

Starting with the thread-aware capture/replay feature, XRT preserves and recreates the application's threading pattern during replay. This ensures:
- Accurate performance measurement for multi-threaded applications
- Correct reproduction of thread-dependent behavior
- Realistic workload characterization

### How It Works

**During Capture:**
1. Each `xrt::run::start()` or `xrt::runlist::execute()` records the calling thread's ID
2. Each `xrt::run::wait()` or `xrt::runlist::wait()` is associated with the calling thread's active frame
3. Thread IDs are exported to `replay.json` in the execution section

**During Replay:**
1. Worker threads are created based on unique thread IDs from capture
2. Frames are grouped and assigned to their corresponding worker threads
3. Each worker thread executes its frames in the original capture order
4. Cross-thread waits are handled through XRT's synchronization primitives

### Threading Model

**Resources (shared across threads):**
- Buffer objects (`xrt::bo`)
- Hardware contexts (`xrt::hw_context`)
- Kernel objects (`xrt::kernel`)
- Run objects (`xrt::run`)
- Runlist objects (`xrt::runlist`)

**Execution (thread-specific):**
- `xrt::run::start()` - Executed in the thread that called it during capture
- `xrt::runlist::execute()` - Executed in the thread that called it during capture
- `xrt::run::wait()` - Executed in the thread's context, can wait on any frame
- `xrt::runlist::wait()` - Executed in the thread's context, can wait on any frame

### Example

Consider a two-threaded application:

**Thread 1:**
```cpp
run1.start();     // Frame 0, Thread 1
run1.wait();
run3.start();     // Frame 2, Thread 1
run3.wait();
```

**Thread 2:**
```cpp
run2.start();     // Frame 1, Thread 2
run2.wait();
```

**Captured JSON:**
```json
{
  "execution": {
    "threads": ["140123456789", "140123456790"],
    "frames": [
      {"name": "frame_0", "tid": "140123456789", "waits": ["frame_0"]},
      {"name": "frame_1", "tid": "140123456790", "waits": ["frame_1"]},
      {"name": "frame_2", "tid": "140123456789", "waits": ["frame_2"]}
    ]
  }
}
```

**During Replay:**
- Two worker threads are created
- Thread 1 executes frame_0, then frame_2
- Thread 2 executes frame_1
- Execution order within each thread is preserved
- Threads run concurrently

## Advanced Topics

### Artifact Deduplication

The capture system automatically deduplicates artifacts:
- Same xclbin used multiple times → single file
- Same buffer data across frames → single dump
- Uses FNV-1a hash for collision detection

### ELF Flow vs Xclbin Flow

Two hardware context creation modes are supported:

**Xclbin Flow** (legacy):
```json
{
  "xclbin": "design.xclbin",
  "ctrlcode": "kernel_ctrl.elf"
}
```

**ELF Flow** (modern):
```json
{
  "programs": ["config.elf", "ctrl.elf"]
}
```

### Module Caching

The replay system caches ELF modules to avoid recreating them:
- Multiple kernels using same control code → single module
- Repository-specific caching prevents conflicts

## See Also

- [recipe.md](recipe.md) - Runner recipe format (superset of replay format)
- [profile.md](profile.md) - Execution profiles
- [README.md](README.md) - Runner infrastructure overview
