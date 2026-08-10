# ELF Representation Migration: XRT → AIEBU

## Background

`xrt::elf` is a first-class XRT object exposing ELF-based AI Engine binaries
to users.  Its internal implementation (`elf_impl`, `elf_int.h`, `xrt_elf.cpp`)
owns an `ELFIO::elfio` object and does all ELF parsing directly in XRT.  AIEBU
already owns a copy of ELFIO and has ELF *writing* infrastructure; it is the
natural owner of all ELF knowledge long-term.

This spec describes the phased migration from "XRT owns the ELF representation"
to "AIEBU owns the ELF representation and XRT wraps it."

---

## Guiding Principles

- **Zero public ABI impact** — `xrt::elf` and its nested types are unchanged
  throughout all phases.
- **`elf_impl` stays** — it is too widely referenced inside XRT to remove in
  phase 1; it becomes a thin delegation shell rather than the ELF parsing core.
- **No ELFIO in XRT** is the measurable exit criterion for phase 1.
- **Incremental** — each step compiles and passes tests independently.

---

## Phase 1 — Eliminate ELFIO from XRT

**Goal:** `#include <elfio/elfio.hpp>` appears nowhere in XRT source. `elf_impl`
delegates all ELF operations to an `aiebu::elf` object. `xrt::elf` public API
is unchanged.

### Step 1.1 — Add `aiebu::elf` reader to AIEBU

**Where:** AIEBU repo (`aiebu/src/cpp/include/aiebu/elf.h` +
`aiebu/src/cpp/elf/elf_reader.cpp`)

AIEBU currently has only ELF *writers*. This step adds a read-side class.
The API must be sufficient to replace every `m_elfio` call site in
`elf_impl`.

**Required `aiebu::elf` API (phase 1 scope only):**

```cpp
namespace aiebu {

class elf {
public:
  // Construction — mirrors xrt::elf constructors
  explicit elf(const std::string& filename);
  explicit elf(std::istream&);
  elf(const void* data, size_t size);
  explicit elf(std::string_view data);

  // ---- Platform identity ----

  // Canonical source for AIE platform identification.
  // Replaces both xrt::elf::platform and the aiebu::osabi_* constants.
  // Values match ELF e_ident[EI_OSABI].
  enum class platform : uint8_t {
    aie2p         = 69,
    aie2ps_legacy = 70,
    aie2ps        = 64,
    aie4          = 75,
    aie4a         = 86,
    aie4z         = 105
  };
  platform get_platform() const;
  uint8_t  get_os_abi() const;   // raw byte, for cases that need it

  // ABI version encoded in e_ident[EI_ABIVERSION]
  // Returns (major, minor): upper nibble = major, lower nibble = minor
  std::pair<uint8_t, uint8_t> get_abi_version() const;

  // ---- Shape queries ----
  bool is_full_elf() const;    // has .ctrltext — can replace xclbin
  bool is_group_elf() const;   // uses .group sections (version-dependent)

  // ---- Metadata embedded in the ELF ----
  std::array<uint8_t, 16> get_cfg_uuid() const;
  uint32_t                get_partition_size() const;

  // ---- Kernel metadata (semantic, not section-walking) ----
  struct arg {
    std::string name;
    std::string data_type;
    uint32_t    index;
  };
  struct kernel {
    std::string              name;
    std::vector<arg>         args;
    std::vector<std::string> instances;
  };
  std::vector<kernel> get_kernels() const;

  // ---- Section access ----

  // Named access for sections AIEBU does not model explicitly.
  // Backing memory is owned by this elf object — span valid for
  // the lifetime of the elf.
  std::span<const std::byte> get_section(std::string_view name) const;

  // Serialise the ELF to a stream (replaces elfio.save() call sites
  // in xdp elf_helper.cpp and xrt_kernel.cpp AIEDebug).
  void save(std::ostream&) const;

  // ---- Patch-point access (transitional — see Phase 2) ----

  // Schema values match the addend encoding used in the .rela sections.
  enum class patch_schema : uint32_t { scaler_32, offset_64 /* ... */ };

  // buf_type identifies which section a patch point targets.
  // Values and names mirror xrt_core::elf_patcher::buf_type — they are
  // kept in sync until elf_patcher.h is retired in Phase 2.
  enum class buf_type : uint32_t {
    ctrltext = 0, ctrldata = 1, preempt_save = 2,
    preempt_restore = 3, pdi = 4, ctrlpkt_pm = 5,
    pad = 6, dump = 7, ctrlpkt = 8
  };

  struct patch_point {
    std::string  arg_name;
    patch_schema schema;
    buf_type     target_buf;
    uint64_t     section_offset;
    uint64_t     base_bo_offset;  // upper bits of rela addend
  };

  // Returns all relocation-based patch points in the ELF.
  // Grouped by ctrl-code-id (group index / UINT32_MAX for legacy ELFs).
  // This method is TRANSITIONAL — it exists only while patching lives in XRT.
  // It will be removed in Phase 2 when patching moves to AIEBU.
  std::map<uint32_t, std::map<std::string, std::vector<patch_point>>>
  get_patch_points() const;

  // ---- Group / ctrl-code navigation ----

  // Maps needed by elf_impl to preserve its existing internal structure
  // during the transition.  These are intentionally low-level; they will
  // be absorbed into AIEBU's own data model in a later phase.
  std::map<uint32_t, uint32_t>              get_section_to_group_map() const;
  std::map<uint32_t, std::vector<uint32_t>> get_group_to_sections_map() const;
  std::map<std::string, uint32_t>           get_kernel_name_to_id_map() const;

  // Ctrl-code id for a named kernel/subkernel (UINT32_MAX = legacy)
  uint32_t get_ctrlcode_id(const std::string& name) const;

  // PDI symbol lookup (AIE gen2 only)
  // Returns empty span if symbol not found.
  std::span<const std::byte> get_pdi(const std::string& symbol_name) const;

  // Preemption control-packet dynamic symbol set (AIE gen2 only)
  std::set<std::string> get_ctrlpkt_pm_dynsyms() const;

  // Per-symbol preemption ctrl-pkt buffer (AIE gen2 only)
  // Span lifetime tied to this elf object.
  std::span<const std::byte>
  get_ctrlpkt_pm_buf(const std::string& symbol_name) const;
};

} // namespace aiebu
```

**Notes on this API:**
- `save()` replaces the two `elfio_ref.save(ss)` call sites in
  `xdp/profile/plugin/aie_dtrace/ve2/elf_helper.cpp` and
  `xdp/profile/plugin/aie_profile/ve2/elf_helper.cpp` without exposing the
  raw `ELFIO::elfio`.
- `get_patch_points()` is explicitly transitional (see comment above).  It
  encodes the same information currently derived from `.rela` + `.symtab`
  sections in `xrt_elf.cpp`, but using AIEBU-owned types so `elf_impl` no
  longer needs ELFIO at the call site.
- The group / ctrl-code maps are low-level scaffolding to keep `elf_impl`'s
  existing internal structure intact during phase 1.  They will be hidden
  behind a higher-level API in a later phase.

**Exit criterion:** `aiebu::elf` builds, parses an AIE ELF, and its unit tests
cover all getter methods.

---

### Step 1.2 — Replace `ELFIO::elfio m_elfio` in `elf_impl` with `aiebu::elf`

**Where:** `elf_int.h`, `xrt_elf.cpp`

Replace the protected member:

```cpp
// before
ELFIO::elfio m_elfio;

// after
aiebu::elf m_elf;
```

Change the `elf_impl` constructor from taking `ELFIO::elfio&&` to taking
`aiebu::elf&&`:

```cpp
// before
elf_impl(ELFIO::elfio&& elfio, elf::platform platform, std::string path);

// after
elf_impl(aiebu::elf&& elf, std::string path);
// platform is now derived from m_elf.get_platform()
```

The four `xrt::elf` constructors in `xrt_elf.cpp` currently construct an
`ELFIO::elfio`, detect the platform, then call `make_elf_impl(elfio, platform,
path)`.  They instead construct an `aiebu::elf`, then call
`make_elf_impl(aiebu_elf, path)`.

All methods on `elf_impl` that currently access `m_elfio` are rewritten to
call the corresponding `aiebu::elf` accessor.  For example:

| Before | After |
|--------|-------|
| `m_elfio.get_os_abi()` | `m_elf.get_os_abi()` |
| `m_elfio.sections[...]->get_data()` | `m_elf.get_section(name)` |
| `m_elfio.get_os_abi()` in `get_elfio()` | removed — see Step 1.3 |
| `parse_sections()` body | replaced by calls to `m_elf.get_*_map()` |

`parse_sections()` currently walks `m_elfio.sections` to build the group maps,
kernel maps, custom section map, and patcher map.  After this step,
`parse_sections()` calls the map accessors from `aiebu::elf` and populates the
same `elf_impl` members.  This keeps `xrt_module.cpp` and other consumers of
`elf_impl` unchanged.

`m_platform` is set from `m_elf.get_platform()` and cast to `xrt::elf::platform`
(same numeric values, so a `static_cast<uint8_t>` round-trip is safe).

**`buf` struct in `elf_int.h`:** Currently `buf::append_section_data()` takes
`const ELFIO::section*`.  After this step it takes `std::span<const std::byte>`
from `aiebu::elf::get_section()`.  The ELFIO-typed overloads are removed.

**Exit criterion:** `xrt_elf.cpp` and `elf_int.h` no longer include
`<elfio/elfio.hpp>`.  XRT builds and existing tests pass.

---

### Step 1.3 — Remove `get_elfio()` from `elf_impl`; fix its two call sites

**Where:** `elf_int.h`, `xrt_kernel.cpp`, `xdp/.../aie_dtrace/ve2/elf_helper.cpp`,
`xdp/.../aie_profile/ve2/elf_helper.cpp`

`get_elfio()` is currently the only method that leaks `ELFIO::elfio` outside
`elf_impl`.  It has exactly two call sites:

1. **`xrt_kernel.cpp:5199`** — `aiebu::AIEDebug dbg(elf_impl_ptr->get_elfio())`  
   Fix: change `aiebu::AIEDebug` to accept `const aiebu::elf&` instead of
   `const ELFIO::elfio&`.  The constructor change is in AIEBU.  The XRT call
   site becomes `aiebu::AIEDebug dbg(elf_impl_ptr->get_aiebu_elf())` where
   `get_aiebu_elf()` returns `const aiebu::elf&`.  
   Alternatively, once AIEBU controls the ELF, `AIEDebug` can be constructed
   from the `aiebu::elf` handle directly without going through `elf_impl` at
   all.

2. **`xdp/.../elf_helper.cpp` (both dtrace and aie_profile)**  
   Both files do:
   ```cpp
   auto* impl = static_cast<xrt::elf_impl*>(elf_handle);
   auto& elfio_ref = impl->get_elfio();
   const_cast<ELFIO::elfio&>(elfio_ref).save(ss);
   ```
   Fix: add `void save(std::ostream&) const` to `elf_impl` (delegates to
   `m_elf.save()`).  The xdp call sites become:
   ```cpp
   auto* impl = static_cast<xrt::elf_impl*>(elf_handle);
   impl->save(ss);
   ```
   The ELFIO `const_cast` disappears.

After these changes, remove `get_elfio()` from `elf_impl` and its declaration
in `elf_int.h`.

**Exit criterion:** `get_elfio()` does not exist anywhere in XRT.  
`<elfio/elfio.hpp>` does not appear in any XRT source file.

---

### Step 1.4 — Remove XRT's bundled ELFIO copy

**Where:** CMakeLists files, XRT's ELFIO source tree

With no remaining `#include <elfio/elfio.hpp>` in XRT, the bundled ELFIO copy
can be removed from the XRT source tree.  AIEBU's own ELFIO copy is the sole
remaining instance.

Check that no CMakeLists target in XRT adds XRT's ELFIO include path.  Remove
the XRT ELFIO source directory.

**Exit criterion:** XRT source tree contains no copy of ELFIO.  AIEBU source
tree retains its copy and XRT links against AIEBU to get ELF services.

---

### Phase 1 — Summary of changed files

| File | Change |
|------|--------|
| `aiebu/src/cpp/include/aiebu/elf.h` | New: `aiebu::elf` reader class |
| `aiebu/src/cpp/elf/elf_reader.cpp` | New: implementation |
| `core/common/api/elf_int.h` | Replace `ELFIO::elfio m_elfio` with `aiebu::elf m_elf`; remove ELFIO includes; update `buf`; add `save()`, `get_aiebu_elf()`; remove `get_elfio()` |
| `core/common/api/xrt_elf.cpp` | Rewrite factory + `parse_sections()` to use `aiebu::elf`; remove ELFIO includes |
| `xdp/.../aie_dtrace/ve2/elf_helper.cpp` | Replace `get_elfio()` + `elfio.save()` with `elf_impl::save()` |
| `xdp/.../aie_profile/ve2/elf_helper.cpp` | Same |
| `core/common/api/xrt_kernel.cpp` | Replace `AIEDebug(get_elfio())` with `AIEDebug(get_aiebu_elf())` |
| AIEBU: `aiebu/src/cpp/aiebu_debug.h/.cpp` | Change `AIEDebug` constructor to accept `const aiebu::elf&` |
| CMakeLists (XRT) | Remove XRT ELFIO include paths and source dir |

`xrt_module.cpp`, `module_int.h`, `xrt_hw_context.cpp`, `xrt_kernel.cpp`
(other than the `AIEDebug` line), and all xdp profiling code that references
`elf_impl*` opaquely — **no changes required**.  `elf_impl`'s public interface
for those callers is unchanged.

---

## Phase 2 — Remove ELF section details from XRT

**Goal:** XRT no longer references section names (`.ctrltext`, `.ctrldata`,
etc.) or understands ELF relocation encoding.  `elf_patcher.h` is retired.
Patching moves to AIEBU.

This phase is contingent on the patching-in-AIEBU work being ready and is
intentionally left at a higher level.

### 2.1 — Move patching to AIEBU

AIEBU exposes a push-style patching API:

```cpp
// Push runtime values; AIEBU applies patches internally.
void elf::patch(std::string_view arg_name, uint64_t address);

// Return finalized, submission-ready payload after all patches applied.
std::span<const std::byte> elf::get_patched_payload() const;
```

`xrt_module.cpp`'s patching loop replaces `symbol_patcher::patch_symbol()` +
per-arg `xrt::bo` address extraction with `m_elf.patch(name, address)`.

`elf_patcher.h`, `elf_patcher.cpp`, `symbol_patcher`, `patcher_config` are
removed from XRT.

`get_patch_points()` is removed from `aiebu::elf` (it was explicitly
transitional).

### 2.2 — Remove `module_config_aie_gen2` / `module_config_aie_gen2_plus`

The `module_config` variant and associated `buf`/`ctrlcode`/`instr_buf`
scaffolding in `elf_int.h` exist because `xrt_module.cpp` manually assembles
control buffers from named ELF sections.  Once AIEBU owns patching and payload
assembly, `module_run` objects receive a finalized payload from AIEBU rather
than constructing it from raw section views.  The `buf` struct and its
zero-copy scaffolding are removed.

### 2.3 — `xrt::elf::platform` becomes a type alias

With `aiebu::elf::platform` as the canonical type, `xrt::elf::platform` is
declared as:

```cpp
using platform = aiebu::elf::platform;
```

or mapped one-to-one.  The numeric values are identical so no existing user
code breaks.

### 2.4 — `elf_impl` is reduced to an adapter

After phases 2.1–2.3, `elf_impl` retains only:

- `get_ert_opcode()` — maps `aiebu::elf::platform` to `ert_cmd_opcode` (XRT
  driver ABI, cannot move to AIEBU)
- `get_kernel_properties_and_args()` — bridges `aiebu::elf::kernel` to
  `xrt_core::xclbin::kernel_properties` (xclbin type, cannot move to AIEBU)
- Construction and forwarding to `aiebu::elf`

At this point `elf_impl` is ~50 lines.

---

## Phase 3 — `xrt::elf = aiebu::elf` (optional / long-term)

Once `elf_impl` is only an adapter for `ert_cmd_opcode` and the xclbin bridge,
evaluate whether `xrt::elf` can become a direct type alias for `aiebu::elf` with
the two XRT-specific concerns handled at the `xrt_kernel.cpp` / `xrt_module.cpp`
call sites directly.  This phase requires AIEBU to commit to a stable ABI for
`aiebu::elf`, which is a separate AIEBU decision.
