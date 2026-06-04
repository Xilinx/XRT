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
- Waits are associated with the current active frame
- Replay executes waits in the same order as captured
- Handles asynchronous execution correctly

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
    "frames": [
      {
        "name": "<frame_id>",
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

- **frames**: Ordered list of frame executions
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
