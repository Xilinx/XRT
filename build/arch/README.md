# Arch Linux Packaging for XRT

This directory contains PKGBUILDs for creating Arch Linux packages from XRT builds.

## Prerequisites

1. Install build dependencies:
```bash
cd ../..
sudo ./src/runtime_src/tools/scripts/xrtdeps.sh
```

2. Build XRT:
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

# Install xrt-base first (required dependency)
sudo pacman -U ./xrt-base-[0-9]*-x86_64.pkg.tar.zst

# Build xrt-npu package
makepkg -p PKGBUILD-xrt-npu
```

## Installing Packages

```bash
sudo pacman -U ./xrt-npu-[0-9]*-x86_64.pkg.tar.zst
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

## Notes

- The `xrt-npu` package depends on `xrt-base` of the matching version
- Version is automatically derived from the built tarball filename
- Use `makepkg --nobuild` to verify PKGBUILD without building