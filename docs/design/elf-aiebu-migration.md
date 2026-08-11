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

#### Implementation notes (step 1.1)

**Files:** `aiebu/src/cpp/include/aiebu/elf.h`,
`aiebu/src/cpp/elf/elf_reader.cpp`, `aiebu/src/cpp/CMakeLists.txt`

**Internal structure — base + subclass pattern:**  
The implementation uses a non-public `elf_reader` abstract base with two
concrete subclasses: `elf_reader_aie2p` (AIE2P / gen2) and
`elf_reader_gen2plus` (AIE2PS, AIE4 family).  This mirrors XRT's
`elf_impl` / `elf_aie_gen2` / `elf_aie_gen2_plus` hierarchy.  `aiebu::elf`
holds a `unique_ptr<elf_reader>` and delegates all calls to it.

**API additions vs. spec:**

- `get_ctrl_scratch_pad_mem_size() const → size_t` — added as a public
  getter.  The value was parsed from `.dynsym` in XRT but only surfaced
  inside `module_config_aie_gen2`.  Exposing it here avoids re-parsing
  in Step 1.2.

- `elf::arg` has an extra `bool is_global` field beyond the spec's
  `{name, data_type, index}`.  This mirrors the `argtype::global` flag in
  XRT's `kernel_argument` and is needed for Step 1.2 to reconstruct the
  XRT `xarg` without re-parsing.

**API deviations vs. spec:**

- `patch_schema` enumerator spelling: spec says `scaler_32` / `offset_64`;
  implementation uses `scalar_32bit` / `offset_64bit` to match
  `xrt_core::elf_patcher::symbol_type` names exactly.  The underlying
  integer values are the same.

- `patch_point` has an extra `uint32_t mask` field (register-value mask for
  `scalar_32bit` patches, zero otherwise).  Needed to carry the `st_size`
  field from the `.dynsym` entry that XRT uses when constructing
  `patch_config`.

- `buf_type` has an extra enumerator `buf_type_count` used as a
  sentinel during gen2plus patcher init.

**Internal additions not in spec:**

- `detect_platform()` — validates the OS/ABI byte and throws on unknown
  values.  XRT's factory did a bare `static_cast` without validation.

- `resolve_ctrlcode_id(lambda)` on the base class — refactors the
  duplicated `get_ctrlcode_id` body from XRT's two derived classes into one
  parameterised helper.

- `section_buf` (internal) — a stripped-down version of XRT's `buf`:
  retains `append`, `pad_to`, `size`, `copy_to`.  The `to_bytes()`
  materialisation helper that was present in an earlier draft was removed
  (see memory note below).

**PDI / ctrlpkt_pm API — size+copy instead of span:**  
The spec showed `get_pdi()` / `get_ctrlpkt_pm_buf()` returning
`span<const std::byte>`.  The implementation instead exposes size+copy
pairs (`get_pdi_size` / `copy_pdi` and `get_ctrlpkt_pm_buf_size` /
`copy_ctrlpkt_pm_buf`).  Every caller in `xrt_module.cpp` allocates a BO
of the reported size and immediately copies into it; a `span` would have
required an intermediate heap allocation to coalesce the multi-view
`section_buf` into contiguous memory before the copy.  The size+copy API
eliminates that allocation: `copy_to()` streams directly from ELFIO memory
into the BO mapping.  Consequently `m_byte_cache` and `materialise()` are
absent from the implementation entirely.

**Memory footprint — identical to XRT:**  
- All section buffer maps use zero-copy `string_view`s into ELFIO memory,
  exactly as XRT's `buf` does.
- Custom sections are stored as zero-copy spans into ELFIO memory (same as
  XRT's `parse_custom_sections`).
- `get_section()` returns a direct span into ELFIO memory; no copy is made.
- PDI and ctrlpkt_pm data is copied once, directly into the caller-supplied
  BO mapping via `copy_to()`; no intermediate buffer exists.

**Parsing fidelity:**  
All section-parsing logic (`init_legacy_section_maps`,
`parse_single_group_section`, `parse_sections`, both `.rela.dyn` walkers,
`init_column_ctrlcode`) is a direct port from XRT with no behavioural
changes.  The addend-decode constants (`addend_shift`, `addend_mask`,
`schema_mask`) and the key-string format for patch-point lookup are
intentionally identical to `xrt_core::elf_patcher` so Step 1.2 can consume
`get_patch_points()` without changing `xrt_module.cpp`.

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

**`parse_sections()` and group/kernel maps:**  
`parse_sections()` currently walks `m_elfio.sections` to build the group maps,
kernel maps, and custom section map.  After this step it is replaced by three
direct assignments from `aiebu::elf`:

```cpp
m_section_to_group_map   = m_elf.get_section_to_group_map();
m_group_to_sections_map  = m_elf.get_group_to_sections_map();
m_kernel_name_to_id_map  = m_elf.get_kernel_name_to_id_map();
```

`m_kernels` is populated from `m_elf.get_kernels()`, translating each
`aiebu::elf::kernel` (plain struct with `name`, `args`, `instances`) into the
existing `xrt::elf::kernel` pimpl objects.  The `aiebu::elf::arg::is_global`
field drives the `xrt::xarg::argtype` discriminant without re-parsing.

`m_custom_section_map` is populated by iterating `m_elf.get_section()` for
each key already known to `elf_impl`; alternatively, the map can be dropped
from `elf_impl` entirely and `get_custom_section()` can delegate to
`m_elf.get_section()` directly.

`m_kernel_args_map`, `m_kernel_to_subkernels_map`, and all the private parsing
helpers (`get_symbol_from_symtab`, `get_kernel_subkernel_from_symtab`,
`init_legacy_section_maps`, `parse_single_group_section`,
`parse_custom_sections`, `finalize_kernels`) are removed from `elf_impl` —
this logic now lives exclusively in `aiebu::elf`.

**`m_arg2patcher` — translation from `aiebu::elf::get_patch_points()`:**  
The patcher table is the most important member to get right because
`get_patcher_configs()` is called at `module_run` construction and
`m_patcher_configs` is used on every `set_arg_value()` call on the hot path.
The structure of `m_arg2patcher` is unchanged — it remains a
`map<ctrl_code_id, map<key_string, patcher_config>>` owned by `elf_impl` with
`module_run` holding a raw `const*` pointer into it.  Only the population
changes: instead of the `.rela.dyn` walking loop, `initialize_arg_patchers()`
translates from `m_elf.get_patch_points()`:

```cpp
for (const auto& [grp_idx, key_map] : m_elf.get_patch_points()) {
  for (const auto& [key, pts] : key_map) {
    std::vector<patch_config> configs;
    for (const auto& pp : pts)
      configs.push_back({pp.section_offset, pp.base_bo_offset, pp.mask});
    m_arg2patcher[grp_idx].emplace(
      key,
      patcher_config{static_cast<patcher_symbol_type>(pp.schema), configs,
                     static_cast<patcher_buf_type>(pp.target_buf)});
  }
}
```

The key-string format and the `patcher_config` / `patch_config` field layout
are intentionally identical to what the old parsing code produced (verified
during step 1.1 implementation), so `module_run`'s hot-path map lookup and
`symbol_patcher::patch_symbol()` are completely unchanged.

**Critical path — no runtime impact:**  
`get_patcher_configs()` returns a raw `const*` into `m_arg2patcher` exactly as
before.  `symbol_patcher` holds a raw `const patcher_config*` and writes
directly into BO mappings.  The data layout and pointer relationships are
identical; only the construction-time code that fills `m_arg2patcher` changes.
Memory footprint of `m_arg2patcher` is identical — same map structure, same
`patcher_config` objects with the same `patch_config` vectors.

**Section buffer maps (`m_instr_buf_map`, `m_ctrlcodes_map`, etc.):**  
`aiebu::elf` does not expose its internal `section_buf` maps — these remain
owned by `elf_aie_gen2` and `elf_aie_gen2_plus` in XRT.  The buffer init
functions (`initialize_section_buf_map`, `initialize_column_ctrlcode`, etc.)
are rewritten to iterate the group/section maps obtained from `aiebu::elf`
rather than walking `m_elfio.sections` directly:

```cpp
// before: iterating m_elfio.sections
for (const auto& sec : m_elfio.sections) { ... sec->get_data() ... }

// after: iterating via aiebu::elf maps + get_section()
for (const auto& [grp_idx, sec_ids] : m_elf.get_group_to_sections_map()) {
  for (auto sec_idx : sec_ids) {
    // resolve section name from the index — requires a new
    // get_section_name(uint32_t index) accessor on aiebu::elf,
    // or alternatively the section names are stored in the
    // group map during aiebu::elf parsing (preferred).
  }
}
```

This reveals a missing accessor: `aiebu::elf` must expose section names by
index so `elf_aie_gen2` / `elf_aie_gen2_plus` can identify which buffer type
each section belongs to without holding `m_elfio` themselves.  Add to
`aiebu::elf`:

```cpp
// Returns the name of section at index, or empty string if not found.
std::string get_section_name(uint32_t index) const;
```

Alternatively, the buffer maps can be built during `aiebu::elf` parsing and
exposed through typed accessors — but that is Phase 2 scope.  For Step 1.2,
`get_section_name(uint32_t)` is the minimal addition required.

**`buf::append_section_data()` signature:**  
Currently takes `const ELFIO::section*`.  After this step it takes
`aiebu::detail::span<const std::byte>` (from `m_elf.get_section(name)`).
The `unique_ptr<ELFIO::section>` overload is also removed.  `buf` itself
(`elf_int.h`) gains no ELFIO dependency.

**`get_pdi()` on `elf_impl` / `module_config_aie_gen2::elf_parent`:**  
Currently `elf_aie_gen2::get_pdi()` returns `const buf&` — a zero-copy
reference into its own `m_pdi_buf_map`.  After Step 1.2, PDI data lives
inside `aiebu::elf`.  Replace the `get_pdi()` virtual with a size+copy pair
on `elf_impl` that forwards to `m_elf`:

```cpp
size_t get_pdi_size(const std::string& symbol) const;
void   copy_pdi(const std::string& symbol, aiebu::detail::span<std::byte> dest) const;
```

`xrt_module.cpp` call sites change from:
```cpp
const auto& pdi_data = m_config.elf_parent->get_pdi(symbol);
auto pdi_bo = xbi::create_bo(m_hwctx, pdi_data.size(), ...);
fill_bo_with_data(pdi_bo, pdi_data);
```
to:
```cpp
auto sz = m_config.elf_parent->get_pdi_size(symbol);
auto pdi_bo = xbi::create_bo(m_hwctx, sz, ...);
m_config.elf_parent->copy_pdi(symbol, {pdi_bo.map<std::byte*>(), sz});
```

The `elf_parent` back-pointer in `module_config_aie_gen2` is retained for
this step and removed in Step 2 when `module_config` is retired.  Similarly,
`get_ctrlpkt_pm_buf_size()` / `copy_ctrlpkt_pm_buf()` replace the
`m_ctrlpkt_pm_bufs` map reference in `module_config_aie_gen2`.

`m_platform` is set from `m_elf.get_platform()` and cast to `xrt::elf::platform`
(same numeric values, so a `static_cast<uint8_t>` round-trip is safe).

**`buf` struct in `elf_int.h`:** Currently `buf::append_section_data()` takes
`const ELFIO::section*`.  After this step it takes
`aiebu::detail::span<const std::byte>` from `m_elf.get_section()`.
The ELFIO-typed overloads are removed.  `buf` gains no ELFIO dependency.

**Missing `aiebu::elf` accessor — `get_section_name(uint32_t)`:**  
As noted above, this must be added to `aiebu::elf` and `elf_reader` before
Step 1.2 can proceed.  It is a trivial addition: `m_elfio.sections[index]`
returns the section; return its name or empty string.  This should be
committed to the aiebu `elfio_migration` branch as a preparatory step.

**Exit criterion:** `xrt_elf.cpp` and `elf_int.h` no longer include
`<elfio/elfio.hpp>`.  XRT builds and existing tests pass.

#### Implementation notes (step 1.2)

**Files changed:** `elf_int.h`, `xrt_elf.cpp`, `xrt_module.cpp`,
`xrt_kernel.cpp`, `aiebu/src/cpp/include/aiebu/elf.h`,
`aiebu/src/cpp/elf/elf_reader.cpp`

**`elf_impl` — constructor and data members:**  
`m_elfio` is replaced by `m_elf` (type `aiebu::elf`).  The constructor now
takes `aiebu::elf&&` and derives `m_platform` from `m_elf.get_os_abi()`.
`m_kernels` and `m_arg2patcher` are initialised directly in the initializer
list via two static helpers (`create_kernels`, `create_arg2patcher`) defined
in the anonymous namespace before `namespace xrt` — required so they are
visible at the point of use in the initializer list.

**`parse_sections()` eliminated:**  
The method is removed entirely.  Kernel translation (`create_kernels`) and
patcher-table construction (`create_arg2patcher`) are now static free
functions called from the `elf_impl` constructor initializer list.

**Group/section maps not copied:**  
The spec proposed copying `m_section_to_group_map`, `m_group_to_sections_map`,
and `m_kernel_name_to_id_map` into `elf_impl`.  These were eliminated — no
XRT consumer read them after construction; all access goes through
`aiebu::elf` virtual accessors instead.

**`buf` struct removed from `elf_int.h`:**  
`struct buf`, `instr_buf`, `control_packet`, `ctrlcode` type aliases, and all
ELFIO-typed overloads of `append_section_data()` are removed.  The module_run
buffer creation path now calls `elf_impl::get_instr_buf_size()` /
`copy_instr_buf()` etc. directly (see buffer accessor section below).

**`elf_aie_gen2` and `elf_aie_gen2_plus` — gutted to shells:**  
All buffer maps (`m_instr_buf_map`, `m_ctrlcodes_map`, etc.), all buffer
initialisation methods, all `.rela.dyn` walking, and `get_module_config()`
are removed.  Each derived class now contains only a constructor (trace point
only, body empty), `is_group_elf()`, and `get_ert_opcode()`.
`get_ctrlcode_id()` was promoted to a non-virtual on `elf_impl` since both
overrides were identical.

**Buffer accessor API on `elf_impl`:**  
All buffer access uses size+copy pairs forwarding directly to `aiebu::elf`
virtual methods (`get_instr_buf_size`/`copy_instr_buf`, etc.).  `module_run`
calls these with the `ctrl_code_id` it already holds, allocates a BO, and
copies directly into the BO mapping — no intermediate heap buffer.

**`module_config_aie_gen2/plus` and `get_module_config()` removed:**  
Both config structs and the `get_module_config()` pure virtual are gone.
`module_run_aie_gen2` and `module_run_aie_gen2_plus` hold only `m_elf_impl`
and call buffer accessors directly.  `elf_parent` back-pointer eliminated.

**`has_pdi()` added to `aiebu::elf`:**  
`get_ert_opcode()` originally checked `!m_pdi_buf_map.empty()` — true if any
`.pdi.*` section is present in the ELF, regardless of relocations.  An
intermediate draft used `get_patch_points()` iteration (O(N)) and then
`!m_ctrl_pdi_map.empty()` (only true when a relocation references a PDI
symbol), both of which are subtly different from the original.  The final
implementation backs `has_pdi()` with `!m_pdi_buf_map.empty()`, which is
faithful to the original semantic and O(1).

**`aiebu::detail::span` relocated:**  
`detail/span.h` moved to `aiebu/detail/span.h` so that consumers linking
against `aiebu_static` can reach it via the aiebu include interface.

**ELFIO transitional include in `elf_int.h`:**  
`<elfio/elfio.hpp>` is re-added to `elf_int.h` with a transitional comment
because external submodules (`xdna-driver` shim tests, `xdp` elf_helpers)
access `ELFIO::elfio` through `elf_impl::get_elfio()`.  Removed in Step 1.3.

**`get_elfio()` escape hatch:**  
`get_elfio()` is added to both `aiebu::elf` (returning `const ELFIO::elfio&`)
and `elf_impl` (forwarding to `m_elf.get_elfio()`).  Marked transitional;
removed in Step 1.3.

**`get_patch_points()` — no copy, cleared after use:**  
`get_patch_points()` now returns `const&` into `elf_reader::m_patch_points`
(zero allocation).  `create_arg2patcher()` iterates this reference once to
build `m_arg2patcher`, then the `elf_impl` constructor calls
`m_elf.clear_patch_points()` to free `m_patch_points` immediately.  This
eliminates the steady-state duplication that would otherwise exist between
`m_patch_points` and `m_arg2patcher` for the ELF's lifetime.

The type boundary between `aiebu::elf::patch_point` and
`xrt_core::elf_patcher::patch_config` means `m_arg2patcher` cannot reference
`m_patch_points` data directly — a translation copy is unavoidable while
patching lives in XRT.  Both `m_patch_points` and `m_arg2patcher` are
**Phase 1 only**: Phase 2 removes `m_arg2patcher` (patching moves to AIEBU)
and with it the need for `m_patch_points` in AIEBU at all.

**`get_pdi_symbols(uint32_t ctrl_code_id)` added to `aiebu::elf`:**  
`module_run_aie_gen2::create_instruction_buf()` previously called
`get_patch_points()` and walked all groups and all relocations to find PDI
symbols for `m_ctrl_code_id` — O(N) with a deep copy.  `get_pdi_symbols()`
returns `const std::unordered_set<std::string>&` into `m_ctrl_pdi_map`
directly indexed by `ctrl_code_id` — O(1), zero allocation.

**Critical path unchanged:**  
`m_arg2patcher` layout, `get_patcher_configs()` raw-pointer return, and
`symbol_patcher::patch_symbol()` are byte-for-byte identical to Step 1.1.
The construction-time translation (`create_arg2patcher`) runs once and is
off the hot path.

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
