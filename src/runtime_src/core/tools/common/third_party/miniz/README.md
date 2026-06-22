# miniz (vendored)

Embedded copy of [miniz](https://github.com/richgel999/miniz) **3.1.1** (MIT), linked into **xrt-smi** and **xbmgmt** with `XBUtilities.cpp` to optionally zlib-inflate archive members when `extract_artifacts_from_archive(..., is_compressed=true)` (e.g. validate sanity from VTD `build_archives.py`).

Upstream: https://github.com/richgel999/miniz/releases/tag/3.1.1
