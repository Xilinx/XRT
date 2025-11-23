# Arch Linux Packaging for XRT

This directory contains PKGBUILDs for creating Arch Linux packages from XRT builds.

## Prerequisites

1. Build XRT first:
```bash
cd ..
./build.sh -npu -opt
```

This will create tarballs in `build/Release/`:
- `xrt_*--base.tar.gz`
- `xrt_*--npu.tar.gz`

## Building Packages

From this directory (`build/arch/`):

```bash
# Build xrt-base package
makepkg -p PKGBUILD-xrt-base

# Build xrt-npu package
makepkg -p PKGBUILD-xrt-npu
```

## Installing Packages

```bash
# Install xrt-base first (required dependency)
sudo pacman -U xrt-base-*.pkg.tar.zst

# Then install xrt-npu
sudo pacman -U xrt-npu-*.pkg.tar.zst
```

## Customizing Build Directory

If your XRT build directory is not at the default location (`../Release`), set the `XRT_BUILD_DIR` environment variable:

```bash
XRT_BUILD_DIR=/path/to/build/Release makepkg -p PKGBUILD-xrt-base
```

## Package Information

- **xrt-base**: Core XRT libraries and tools
- **xrt-npu**: NPU-specific XRT components (requires xrt-base)

Both packages install to `/opt/xilinx/xrt/` following XRT's standard installation layout.
