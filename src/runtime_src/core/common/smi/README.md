<!--
SPDX-License-Identifier: Apache-2.0
Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
-->

# xrt-smi

`xrt-smi` is the XRT command-line tool for Ryzen client NPUs (among other targets). On Ryzen it provides **examine**, **validate**, and **configure** subcommands. The tables below summarize the usual options and choices for those subcommands.

For every flag and report or test name supported on your system, run `xrt-smi <subcommand> --help`.

---

## examine

Options for `xrt-smi examine` on Ryzen NPUs:

| Option | Short | Description |
|--------|-------|-------------|
| `--device` | `-d` | PCI Bus:Device.Function of the device (for example `0000:d8:00.0`). |
| `--format` | `-f` | Report format: `JSON` (default) or `JSON-2020.2`. |
| `--output` | `-o` | Write report output to the given file. |
| `--help` | `-h` | Show subcommand help. |
| `--report` | `-r` | One or more reports (see the next table). |

Values for `--report` / `-r`:

| Report | Description |
|--------|-------------|
| `aie-partitions` | AIE partition information. |
| `all` | Produce all reports supported for your device. |
| `host` | Host information (default when no report is selected). |
| `platform` | Platform / device summary. |

---

## validate

Options for `xrt-smi validate` on Ryzen NPUs:

| Option | Short | Description |
|--------|-------|-------------|
| `--device` | `-d` | PCI Bus:Device.Function of the device. |
| `--format` | `-f` | Results format: `JSON` (default) or `JSON-2020.2`. |
| `--output` | `-o` | Write results to the given file. |
| `--help` | `-h` | Show subcommand help. |
| `--run` | `-r` | One or more tests (see the next table). |

Values for `--run` / `-r`:

| Test | Description |
|------|-------------|
| `all` | Run all applicable validate tests (default when none are selected). |
| `latency` | End-to-end latency test. |
| `throughput` | End-to-end throughput test. |
| `gemm` | GEMM INT8 workload; reports throughput-oriented results. |

---

## configure

Options for `xrt-smi configure` on Ryzen NPUs:

| Option | Short | Description |
|--------|-------|-------------|
| `--device` | `-d` | PCI Bus:Device.Function of the device. |
| `--help` | `-h` | Show subcommand help. |
| `--pmode` | *(none)* | Power mode: `default`, `powersaver`, `balanced`, `performance`, or `turbo`. |

---

For a broader FleXible RunTime (XRT) overview, see [README.rst](../../../../../README.rst) at the root of the `xrt` tree in this repository.
