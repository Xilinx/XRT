#ifdef __linux__
#include <cstring>

extern "C" {
const char * func_mangled_map[] = {
  "xrt::aie::device::open_context(xrt::aie::device::access_mode)", "_ZN3xrt3aie6device12open_contextENS0_11access_modeE",
  "xrt::aie::device::read_aie_mem(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t)", "_ZNK3xrt3aie6device12read_aie_memEitttjj",
  "xrt::aie::device::read_aie_reg(pid_t, uint16_t, uint16_t, uint16_t, uint32_t)", "_ZNK3xrt3aie6device12read_aie_regEitttj",
  "xrt::aie::device::write_aie_mem(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, const std::vector<char>&)", "_ZN3xrt3aie6device13write_aie_memEitttjRKSt6vectorIcSaIcEE",
  "xrt::aie::device::write_aie_reg(pid_t, uint16_t, uint16_t, uint16_t, uint32_t, uint32_t)", "_ZN3xrt3aie6device13write_aie_regEitttjj",
  "xrt::aie::program::get_partition_size(void)", "_ZNK3xrt3aie7program18get_partition_sizeEv",
  "xrt::aie::program::valid_or_error(void)", "_ZN3xrt3aie7program14valid_or_errorEv",
  "xrt::bo::address(void)", "_ZNK3xrt2bo7addressEv",
  "xrt::bo::async(xclBOSyncDirection, size_t, size_t)", "_ZN3xrt2bo5asyncE18xclBOSyncDirectionmm",
  "xrt::bo::async_handle::wait(void)", "_ZN3xrt2bo12async_handle4waitEv",
  "xrt::bo::bo(const xrt::bo&, size_t, size_t)", "_ZN3xrt2boC2ERKS0_mm",
  "xrt::bo::bo(const xrt::device&, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_6deviceEmNS0_5flagsEj",
  "xrt::bo::bo(const xrt::device&, size_t, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_6deviceEmj",
  "xrt::bo::bo(const xrt::device&, void*, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_6deviceEPvmNS0_5flagsEj",
  "xrt::bo::bo(const xrt::device&, void*, size_t, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_6deviceEPvmj",
  "xrt::bo::bo(const xrt::device&, xrt::bo::export_handle)", "_ZN3xrt2boC2ERKNS_6deviceEi",
  "xrt::bo::bo(const xrt::device&, xrt::pid_type, xrt::bo::export_handle)", "_ZN3xrt2boC2ERKNS_6deviceENS_8pid_typeEi",
  "xrt::bo::bo(const xrt::hw_context&, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_10hw_contextEmNS0_5flagsEj",
  "xrt::bo::bo(const xrt::hw_context&, size_t, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_10hw_contextEmj",
  "xrt::bo::bo(const xrt::hw_context&, void*, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_10hw_contextEPvmNS0_5flagsEj",
  "xrt::bo::bo(const xrt::hw_context&, void*, size_t, xrt::memory_group)", "_ZN3xrt2boC2ERKNS_10hw_contextEPvmj",
  "xrt::bo::bo(xclDeviceHandle, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2EPvmNS0_5flagsEj",
  "xrt::bo::bo(xclDeviceHandle, void*, size_t, xrt::bo::flags, xrt::memory_group)", "_ZN3xrt2boC2EPvS1_mNS0_5flagsEj",
  "xrt::bo::bo(xclDeviceHandle, xclBufferExportHandle)", "_ZN3xrt2boC2EPvi",
  "xrt::bo::bo(xclDeviceHandle, xcl_buffer_handle)", "_ZN3xrt2boC2EPv17xcl_buffer_handle",
  "xrt::bo::bo(xclDeviceHandle, xrt::pid_type, xclBufferExportHandle)", "_ZN3xrt2boC2EPvNS_8pid_typeEi",
  "xrt::bo::bo(xrtBufferHandle)", "_ZN3xrt2boC2EPv",
  "xrt::bo::copy(const xrt::bo&, size_t, size_t, size_t)", "_ZN3xrt2bo4copyERKS0_mmm",
  "xrt::bo::export_buffer(void)", "_ZN3xrt2bo13export_bufferEv",
  "xrt::bo::get_flags(void)", "_ZNK3xrt2bo9get_flagsEv",
  "xrt::bo::get_memory_group(void)", "_ZNK3xrt2bo16get_memory_groupEv",
  "xrt::bo::map(void)", "_ZN3xrt2bo3mapEv",
  "xrt::bo::read(void*, size_t, size_t)", "_ZN3xrt2bo4readEPvmm",
  "xrt::bo::size(void)", "_ZNK3xrt2bo4sizeEv",
  "xrt::bo::sync(xclBOSyncDirection, size_t, size_t)", "_ZN3xrt2bo4syncE18xclBOSyncDirectionmm",
  "xrt::bo::write(const void*, size_t, size_t)", "_ZN3xrt2bo5writeEPKvmm",
  "xrt::bo::~bo(void)", "_ZN3xrt2boD2Ev",
  "xrt::device::device(const std::string&)", "_ZN3xrt6deviceC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::device::device(unsigned int)", "_ZN3xrt6deviceC2Ej",
  "xrt::device::device(xclDeviceHandle)", "_ZN3xrt6deviceC2EPv",
  "xrt::device::error::error(const std::string&)", "_ZN3xrt6device5errorC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::device::error::what(void)", "_ZNK3xrt6device5error4whatEv",
  "xrt::device::get_info(xrt::info::device)", "_ZNK3xrt6device8get_infoENS_4info6deviceE",
  "xrt::device::get_info(xrt::info::device, const xrt::detail::abi&)", "_ZNK3xrt6device8get_infoENS_4info6deviceERKNS_6detail3abiE",
  "xrt::device::get_info_std(xrt::info::device, const xrt::detail::abi&)", "_ZNK3xrt6device12get_info_stdENS_4info6deviceERKNS_6detail3abiE",
  "xrt::device::get_xclbin_section(axlf_section_kind, const xrt::uuid&)", "_ZNK3xrt6device18get_xclbin_sectionE17axlf_section_kindRKNS_4uuidE",
  "xrt::device::get_xclbin_uuid(void)", "_ZNK3xrt6device15get_xclbin_uuidEv",
  "xrt::device::load_xclbin(const axlf*)", "_ZN3xrt6device11load_xclbinEPK4axlf",
  "xrt::device::load_xclbin(const std::string&)", "_ZN3xrt6device11load_xclbinERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::device::load_xclbin(const xrt::xclbin&)", "_ZN3xrt6device11load_xclbinERKNS_6xclbinE",
  "xrt::device::operator xclDeviceHandle(void)", "_ZNK3xrt6devicecvPvEv",
  "xrt::device::register_xclbin(const xrt::xclbin&)", "_ZN3xrt6device15register_xclbinERKNS_6xclbinE",
  "xrt::device::reset(void)", "_ZN3xrt6device5resetEv",
  "xrt::device::~device(void)", "_ZN3xrt6deviceD2Ev",
  "xrt::elf::elf(const std::string&)", "_ZN3xrt3elfC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::elf::elf(const void *, size_t)", "_ZN3xrt3elfC2EPKvm",
  "xrt::elf::elf(std::istream&)", "_ZN3xrt3elfC2ERSi",
  "xrt::elf::get_cfg_uuid(void)", "_ZNK3xrt3elf12get_cfg_uuidEv",
  "xrt::error::error(const xrt::device&, xrtErrorClass)", "_ZN3xrt5errorC2ERKNS_6deviceE13xrtErrorClass",
  "xrt::error::error(xrtErrorCode, xrtErrorTime)", "_ZN3xrt5errorC2Emm",
  "xrt::error::get_error_code(void)", "_ZNK3xrt5error14get_error_codeEv",
  "xrt::error::get_timestamp(void)", "_ZNK3xrt5error13get_timestampEv",
  "xrt::error::to_string(void)", "_ZNK3xrt5error9to_stringB5cxx11Ev",
  "xrt::ext::bo::bo(const xrt::device&, size_t)", "_ZN3xrt3ext2boC2ERKNS_6deviceEm",
  "xrt::ext::bo::bo(const xrt::device&, size_t, xrt::ext::bo::access_mode)", "_ZN3xrt3ext2boC2ERKNS_6deviceEmNS1_11access_modeE",
  "xrt::ext::bo::bo(const xrt::device&, void*, size_t)", "_ZN3xrt3ext2boC2ERKNS_6deviceEPvm",
  "xrt::ext::bo::bo(const xrt::device&, void*, size_t, xrt::ext::bo::access_mode)", "_ZN3xrt3ext2boC2ERKNS_6deviceEPvmNS1_11access_modeE",
  "xrt::ext::bo::bo(const xrt::device&, xrt::pid_type, xrt::bo::export_handle)", "_ZN3xrt3ext2boC2ERKNS_6deviceENS_8pid_typeEi",
  "xrt::ext::bo::bo(const xrt::hw_context&, size_t)", "_ZN3xrt3ext2boC2ERKNS_10hw_contextEm",
  "xrt::ext::bo::bo(const xrt::hw_context&, size_t, xrt::ext::bo::access_mode)", "_ZN3xrt3ext2boC2ERKNS_10hw_contextEmNS1_11access_modeE",
  "xrt::ext::bo::bo(const xrt::hw_context&, xrt::pid_type, xrt::bo::export_handle)", "_ZN3xrt3ext2boC2ERKNS_10hw_contextENS_8pid_typeEi",
  "xrt::ext::kernel::kernel(const xrt::hw_context&, const std::string&)", "_ZN3xrt3ext6kernelC2ERKNS_10hw_contextERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::ext::kernel::kernel(const xrt::hw_context&, const xrt::module&, const std::string&)", "_ZN3xrt3ext6kernelC2ERKNS_10hw_contextERKNS_6moduleERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::fence::export_fence(void)", "_ZN3xrt5fence12export_fenceEv",
  "xrt::fence::fence(const xrt::device&, xrt::fence::access_mode)", "_ZN3xrt5fenceC2ERKNS_6deviceENS0_11access_modeE",
  "xrt::fence::fence(const xrt::device&, xrt::pid_type, xrt::fence::export_handle)", "_ZN3xrt5fenceC2ERKNS_6deviceENS_8pid_typeEi",
  "xrt::fence::fence(const xrt::fence&)", "_ZN3xrt5fenceC2ERKS0_",
  "xrt::fence::fence(std::unique_ptr<xrt_core::fence_handle>)", "_ZN3xrt5fenceC2ESt10unique_ptrIN8xrt_core12fence_handleESt14default_deleteIS3_EE",
  "xrt::fence::fence(xrt::fence&&)", "_ZN3xrt5fenceC2EOS0_",
  "xrt::fence::get_access_mode(void)", "_ZNK3xrt5fence15get_access_modeEv",
  "xrt::fence::get_next_state(void)", "_ZNK3xrt5fence14get_next_stateEv",
  "xrt::fence::wait(const std::chrono::milliseconds&)", "_ZN3xrt5fence4waitERKNSt6chrono8durationIlSt5ratioILl1ELl1000EEEE",
  "xrt::hw_context::add_config(const xrt::elf&)", "_ZN3xrt10hw_context10add_configERKNS_3elfE",
  "xrt::hw_context::get_device(void)", "_ZNK3xrt10hw_context10get_deviceEv",
  "xrt::hw_context::get_mode(void)", "_ZNK3xrt10hw_context8get_modeEv",
  "xrt::hw_context::get_xclbin(void)", "_ZNK3xrt10hw_context10get_xclbinEv",
  "xrt::hw_context::get_xclbin_uuid(void)", "_ZNK3xrt10hw_context15get_xclbin_uuidEv",
  "xrt::hw_context::hw_context(const xrt::device&, const xrt::elf&)", "_ZN3xrt10hw_contextC2ERKNS_6deviceERKNS_3elfE",
  "xrt::hw_context::hw_context(const xrt::device&, const xrt::elf&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode)", "_ZN3xrt10hw_contextC2ERKNS_6deviceERKNS_3elfERKSt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEjSt4lessISD_ESaISt4pairIKSD_jEEENS0_11access_modeE",
  "xrt::hw_context::hw_context(const xrt::device&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode)", "_ZN3xrt10hw_contextC2ERKNS_6deviceERKSt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEjSt4lessISA_ESaISt4pairIKSA_jEEENS0_11access_modeE",
  "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&)", "_ZN3xrt10hw_contextC2ERKNS_6deviceERKNS_4uuidERKSt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEjSt4lessISD_ESaISt4pairIKSD_jEEE",
  "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode)", "_ZN3xrt10hw_contextC2ERKNS_6deviceERKNS_4uuidENS0_11access_modeE",
  "xrt::hw_context::operator xrt_core::hwctx_handle*(void)", "_ZNK3xrt10hw_contextcvPN8xrt_core12hwctx_handleEEv",
  "xrt::hw_context::update_qos(const xrt::hw_context::qos_type&)", "_ZN3xrt10hw_context10update_qosERKSt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEjSt4lessIS7_ESaISt4pairIKS7_jEEE",
  "xrt::hw_context::~hw_context(void)", "_ZN3xrt10hw_contextD2Ev",
  "xrt::ini::set(const std::string&, const std::string&)", "_ZN3xrt3ini3setERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES8_",
  "xrt::ip::create_interrupt_notify(void)", "_ZN3xrt2ip23create_interrupt_notifyEv",
  "xrt::ip::interrupt::disable(void)", "_ZN3xrt2ip9interrupt7disableEv",
  "xrt::ip::interrupt::enable(void)", "_ZN3xrt2ip9interrupt6enableEv",
  "xrt::ip::interrupt::wait(const std::chrono::milliseconds&)", "_ZNK3xrt2ip9interrupt4waitERKNSt6chrono8durationIlSt5ratioILl1ELl1000EEEE",
  "xrt::ip::interrupt::wait(void)", "_ZN3xrt2ip9interrupt4waitEv",
  "xrt::ip::ip(const xrt::device&, const xrt::uuid&, const std::string&)", "_ZN3xrt2ipC2ERKNS_6deviceERKNS_4uuidERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::ip::ip(const xrt::hw_context&, const std::string&)", "_ZN3xrt2ipC2ERKNS_10hw_contextERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::ip::read_register(uint32_t)", "_ZNK3xrt2ip13read_registerEj",
  "xrt::ip::write_register(uint32_t, uint32_t)", "_ZN3xrt2ip14write_registerEjj",
  "xrt::kernel::get_name(void)", "_ZNK3xrt6kernel8get_nameB5cxx11Ev",
  "xrt::kernel::get_xclbin(void)", "_ZNK3xrt6kernel10get_xclbinEv",
  "xrt::kernel::group_id(int)", "_ZNK3xrt6kernel8group_idEi",
  "xrt::kernel::kernel(const xrt::device&, const xrt::uuid&, const std::string&, xrt::kernel::cu_access_mode)", "_ZN3xrt6kernelC2ERKNS_6deviceERKNS_4uuidERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS0_14cu_access_modeE",
  "xrt::kernel::kernel(const xrt::hw_context&, const std::string&)", "_ZN3xrt6kernelC2ERKNS_10hw_contextERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::kernel::kernel(xclDeviceHandle, const xrt::uuid&, const std::string&, xrt::kernel::cu_access_mode)", "_ZN3xrt6kernelC2EPvRKNS_4uuidERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS0_14cu_access_modeE",
  "xrt::kernel::offset(int)", "_ZNK3xrt6kernel6offsetEi",
  "xrt::kernel::read_register(uint32_t)", "_ZNK3xrt6kernel13read_registerEj",
  "xrt::kernel::write_register(uint32_t, uint32_t)", "_ZN3xrt6kernel14write_registerEjj",
  "xrt::kernel::~kernel(void)", "_ZN3xrt6kernelD2Ev",
  "xrt::mailbox::get_arg(int)", "_ZNK3xrt7mailbox7get_argEi",
  "xrt::mailbox::get_arg_index(const std::string&)", "_ZNK3xrt7mailbox13get_arg_indexERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::mailbox::mailbox(const xrt::run&)", "_ZN3xrt7mailboxC2ERKNS_3runE",
  "xrt::mailbox::read(void)", "_ZN3xrt7mailbox4readEv",
  "xrt::mailbox::set_arg_at_index(int, const void*, size_t)", "_ZN3xrt7mailbox16set_arg_at_indexEiPKvm",
  "xrt::mailbox::set_arg_at_index(int, const xrt::bo&)", "_ZN3xrt7mailbox16set_arg_at_indexEiRKNS_2boE",
  "xrt::mailbox::write(void)", "_ZN3xrt7mailbox5writeEv",
  "xrt::message::detail::enabled(xrt::message::level)", "_ZN3xrt7message6detail7enabledENS0_5levelE",
  "xrt::message::log(xrt::message::level, const std::string&, const std::string&)", "_ZN3xrt7message3logENS0_5levelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES9_",
  "xrt::module::get_cfg_uuid(void)", "_ZNK3xrt6module12get_cfg_uuidEv",
  "xrt::module::get_hw_context(void)", "_ZNK3xrt6module14get_hw_contextEv",
  "xrt::module::module(const xrt::elf&)", "_ZN3xrt6moduleC2ERKNS_3elfE",
  "xrt::module::module(const xrt::module&, const xrt::hw_context&)", "_ZN3xrt6moduleC2ERKS0_RKNS_10hw_contextE",
  "xrt::module::module(void*, size_t, const xrt::uuid&)", "_ZN3xrt6moduleC2EPvmRKNS_4uuidE",
  "xrt::operator==(const xrt::device&, const xrt::device&)", "_ZN3xrteqERKNS_6deviceES2_",
  "xrt::profile::user_event::mark(const char*)", "_ZN3xrt7profile10user_event4markEPKc",
  "xrt::profile::user_event::mark_time_ns(const std::chrono::nanoseconds&, const char*)", "_ZN3xrt7profile10user_event12mark_time_nsERKNSt6chrono8durationIlSt5ratioILl1ELl1000000000EEEEPKc",
  "xrt::profile::user_event::user_event(void)", "_ZN3xrt7profile10user_eventC2Ev",
  "xrt::profile::user_event::~user_event(void)", "_ZN3xrt7profile10user_eventD2Ev",
  "xrt::profile::user_range::end(void)", "_ZN3xrt7profile10user_range3endEv",
  "xrt::profile::user_range::start(const char*, const char*)", "_ZN3xrt7profile10user_range5startEPKcS3_",
  "xrt::profile::user_range::user_range(const char*, const char*)", "_ZN3xrt7profile10user_rangeC2EPKcS3_",
  "xrt::profile::user_range::user_range(void)", "_ZN3xrt7profile10user_rangeC2Ev",
  "xrt::profile::user_range::~user_range(void)", "_ZN3xrt7profile10user_rangeD2Ev",
  "xrt::queue::add_task(xrt::queue::task&&)", "_ZN3xrt5queue8add_taskEONS0_4taskE",
  "xrt::queue::queue(void)", "_ZN3xrt5queueC2Ev",
  "xrt::run::abort(void)", "_ZN3xrt3run5abortEv",
  "xrt::run::add_callback(ert_cmd_state, std::function<void(const void*, ert_cmd_state, void*)>, void*)", "_ZN3xrt3run12add_callbackE13ert_cmd_stateSt8functionIFvPKvS1_PvEES5_",
  "xrt::run::aie_error::aie_error(const xrt::run&, const std::string&)", "_ZN3xrt3run9aie_errorC2ERKS0_RKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::run::aie_error::data(void)", "_ZNK3xrt3run9aie_error4dataEv",
  "xrt::run::command_error::command_error(const xrt::run&, const std::string&)", "_ZN3xrt3run13command_errorC2ERKS0_RKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::run::command_error::command_error(ert_cmd_state, const std::string&)", "_ZN3xrt3run13command_errorC2E13ert_cmd_stateRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::run::command_error::get_command_state(void)", "_ZNK3xrt3run13command_error17get_command_stateEv",
  "xrt::run::command_error::what(void)", "_ZNK3xrt3run13command_error4whatEv",
  "xrt::run::get_arg_index(const std::string&)", "_ZNK3xrt3run13get_arg_indexERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::run::get_ctrl_scratchpad_bo(void)", "_ZNK3xrt3run22get_ctrl_scratchpad_boEv",
  "xrt::run::get_ert_packet(void)", "_ZNK3xrt3run14get_ert_packetEv",
  "xrt::run::return_code(void)", "_ZNK3xrt3run11return_codeEv",
  "xrt::run::run(const xrt::kernel&)", "_ZN3xrt3runC2ERKNS_6kernelE",
  "xrt::run::set_arg_at_index(int, const void*, size_t)", "_ZN3xrt3run16set_arg_at_indexEiPKvm",
  "xrt::run::set_arg_at_index(int, const xrt::bo&)", "_ZN3xrt3run16set_arg_at_indexEiRKNS_2boE",
  "xrt::run::start(const xrt::autostart&)", "_ZN3xrt3run5startERKNS_9autostartE",
  "xrt::run::start(void)", "_ZN3xrt3run5startEv",
  "xrt::run::state(void)", "_ZNK3xrt3run5stateEv",
  "xrt::run::stop(void)", "_ZN3xrt3run4stopEv",
  "xrt::run::submit_signal(const xrt::fence&)", "_ZN3xrt3run13submit_signalERKNS_5fenceE",
  "xrt::run::submit_wait(const xrt::fence&)", "_ZN3xrt3run11submit_waitERKNS_5fenceE",
  "xrt::run::update_arg_at_index(int, const void*, size_t)", "_ZN3xrt3run19update_arg_at_indexEiPKvm",
  "xrt::run::update_arg_at_index(int, const xrt::bo&)", "_ZN3xrt3run19update_arg_at_indexEiRKNS_2boE",
  "xrt::run::wait(const std::chrono::milliseconds&)", "_ZNK3xrt3run4waitERKNSt6chrono8durationIlSt5ratioILl1ELl1000EEEE",
  "xrt::run::wait2(const std::chrono::milliseconds&)", "_ZNK3xrt3run5wait2ERKNSt6chrono8durationIlSt5ratioILl1ELl1000EEEE",
  "xrt::run::~run(void)", "_ZN3xrt3runD2Ev",
  "xrt::runlist::add(const xrt::run&)", "_ZN3xrt7runlist3addERKNS_3runE",
  "xrt::runlist::add(xrt::run&&)", "_ZN3xrt7runlist3addEONS_3runE",
  "xrt::runlist::aie_error::aie_error(const xrt::run&, ert_cmd_state, const std::string&)", "_ZN3xrt7runlist9aie_errorC2ERKNS_3runE13ert_cmd_stateRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::runlist::aie_error::data(void)", "_ZNK3xrt7runlist9aie_error4dataEv",
  "xrt::runlist::command_error::command_error(const xrt::run&, ert_cmd_state, const std::string&)", "_ZN3xrt7runlist13command_errorC2ERKNS_3runE13ert_cmd_stateRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::runlist::command_error::get_command_state(void)", "_ZNK3xrt7runlist13command_error17get_command_stateEv",
  "xrt::runlist::command_error::get_run(void)", "_ZNK3xrt7runlist13command_error7get_runEv",
  "xrt::runlist::command_error::what(void)", "_ZNK3xrt7runlist13command_error4whatEv",
  "xrt::runlist::execute(void)", "_ZN3xrt7runlist7executeEv",
  "xrt::runlist::poll(void)", "_ZNK3xrt7runlist4pollEv",
  "xrt::runlist::reset(void)", "_ZN3xrt7runlist5resetEv",
  "xrt::runlist::runlist(const xrt::hw_context&)", "_ZN3xrt7runlistC2ERKNS_10hw_contextE",
  "xrt::runlist::state(void)", "_ZNK3xrt7runlist5stateEv",
  "xrt::runlist::wait(const std::chrono::milliseconds&)", "_ZNK3xrt7runlist4waitERKNSt6chrono8durationIlSt5ratioILl1ELl1000EEEE",
  "xrt::runlist::~runlist(void)", "_ZN3xrt7runlistD2Ev",
  "xrt::set_read_range(const xrt::kernel&, uint32_t, uint32_t)", "_ZN3xrt14set_read_rangeERKNS_6kernelEjj",
  "xrt::system::enumerate_devices(void)", "_ZN3xrt6system17enumerate_devicesEv",
  "xrt::version::build(void)", "_ZN3xrt7version5buildEv",
  "xrt::version::code(void)", "_ZN3xrt7version4codeEv",
  "xrt::version::feature(void)", "_ZN3xrt7version7featureEv",
  "xrt::version::major(void)", "_ZN3xrt7version5majorEv",
  "xrt::version::minor(void)", "_ZN3xrt7version5minorEv",
  "xrt::version::patch(void)", "_ZN3xrt7version5patchEv",
  "xrt::xclbin::aie_partition::get_inference_fingerprint(void)", "_ZNK3xrt6xclbin13aie_partition25get_inference_fingerprintEv",
  "xrt::xclbin::aie_partition::get_operations_per_cycle(void)", "_ZNK3xrt6xclbin13aie_partition24get_operations_per_cycleEv",
  "xrt::xclbin::aie_partition::get_pre_post_fingerprint(void)", "_ZNK3xrt6xclbin13aie_partition24get_pre_post_fingerprintEv",
  "xrt::xclbin::arg::get_host_type(void)", "_ZNK3xrt6xclbin3arg13get_host_typeB5cxx11Ev",
  "xrt::xclbin::arg::get_index(void)", "_ZNK3xrt6xclbin3arg9get_indexEv",
  "xrt::xclbin::arg::get_mems(void)", "_ZNK3xrt6xclbin3arg8get_memsEv",
  "xrt::xclbin::arg::get_name(void)", "_ZNK3xrt6xclbin3arg8get_nameB5cxx11Ev",
  "xrt::xclbin::arg::get_offset(void)", "_ZNK3xrt6xclbin3arg10get_offsetEv",
  "xrt::xclbin::arg::get_port(void)", "_ZNK3xrt6xclbin3arg8get_portB5cxx11Ev",
  "xrt::xclbin::arg::get_size(void)", "_ZNK3xrt6xclbin3arg8get_sizeEv",
  "xrt::xclbin::get_aie_partitions(void)", "_ZNK3xrt6xclbin18get_aie_partitionsEv",
  "xrt::xclbin::get_axlf(void)", "_ZNK3xrt6xclbin8get_axlfEv",
  "xrt::xclbin::get_axlf_section(axlf_section_kind)", "_ZNK3xrt6xclbin16get_axlf_sectionE17axlf_section_kind",
  "xrt::xclbin::get_fpga_device_name(void)", "_ZNK3xrt6xclbin20get_fpga_device_nameB5cxx11Ev",
  "xrt::xclbin::get_interface_uuid(void)", "_ZNK3xrt6xclbin18get_interface_uuidEv",
  "xrt::xclbin::get_ip(const std::string&)", "_ZNK3xrt6xclbin6get_ipERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::get_ips(const std::string&)", "_ZNK3xrt6xclbin7get_ipsERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::get_ips(void)", "_ZNK3xrt6xclbin7get_ipsEv",
  "xrt::xclbin::get_kernel(const std::string&)", "_ZNK3xrt6xclbin10get_kernelERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::get_kernels(void)", "_ZNK3xrt6xclbin11get_kernelsEv",
  "xrt::xclbin::get_mems(void)", "_ZNK3xrt6xclbin8get_memsEv",
  "xrt::xclbin::get_target_type(void)", "_ZNK3xrt6xclbin15get_target_typeEv",
  "xrt::xclbin::get_uuid(void)", "_ZNK3xrt6xclbin8get_uuidEv",
  "xrt::xclbin::get_xsa_name(void)", "_ZNK3xrt6xclbin12get_xsa_nameB5cxx11Ev",
  "xrt::xclbin::ip::get_arg(int32_t)", "_ZNK3xrt6xclbin2ip7get_argEi",
  "xrt::xclbin::ip::get_args(void)", "_ZNK3xrt6xclbin2ip8get_argsEv",
  "xrt::xclbin::ip::get_base_address(void)", "_ZNK3xrt6xclbin2ip16get_base_addressEv",
  "xrt::xclbin::ip::get_control_type(void)", "_ZNK3xrt6xclbin2ip16get_control_typeEv",
  "xrt::xclbin::ip::get_name(void)", "_ZNK3xrt6xclbin2ip8get_nameB5cxx11Ev",
  "xrt::xclbin::ip::get_num_args(void)", "_ZNK3xrt6xclbin2ip12get_num_argsEv",
  "xrt::xclbin::ip::get_size(void)", "_ZNK3xrt6xclbin2ip8get_sizeEv",
  "xrt::xclbin::ip::get_type(void)", "_ZNK3xrt6xclbin2ip8get_typeEv",
  "xrt::xclbin::kernel::get_arg(int32_t)", "_ZNK3xrt6xclbin6kernel7get_argEi",
  "xrt::xclbin::kernel::get_args(void)", "_ZNK3xrt6xclbin6kernel8get_argsEv",
  "xrt::xclbin::kernel::get_cu(const std::string&)", "_ZNK3xrt6xclbin6kernel6get_cuERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::kernel::get_cus(const std::string&)", "_ZNK3xrt6xclbin6kernel7get_cusERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::kernel::get_cus(void)", "_ZNK3xrt6xclbin6kernel7get_cusEv",
  "xrt::xclbin::kernel::get_name(void)", "_ZNK3xrt6xclbin6kernel8get_nameB5cxx11Ev",
  "xrt::xclbin::kernel::get_num_args(void)", "_ZNK3xrt6xclbin6kernel12get_num_argsEv",
  "xrt::xclbin::kernel::get_type(void)", "_ZNK3xrt6xclbin6kernel8get_typeEv",
  "xrt::xclbin::mem::get_base_address(void)", "_ZNK3xrt6xclbin3mem16get_base_addressEv",
  "xrt::xclbin::mem::get_index(void)", "_ZNK3xrt6xclbin3mem9get_indexEv",
  "xrt::xclbin::mem::get_size_kb(void)", "_ZNK3xrt6xclbin3mem11get_size_kbEv",
  "xrt::xclbin::mem::get_tag(void)", "_ZNK3xrt6xclbin3mem7get_tagB5cxx11Ev",
  "xrt::xclbin::mem::get_type(void)", "_ZNK3xrt6xclbin3mem8get_typeEv",
  "xrt::xclbin::mem::get_used(void)", "_ZNK3xrt6xclbin3mem8get_usedEv",
  "xrt::xclbin::xclbin(const axlf*)", "_ZN3xrt6xclbinC2EPK4axlf",
  "xrt::xclbin::xclbin(const std::string&)", "_ZN3xrt6xclbinC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin::xclbin(const std::string_view&)", "_ZN3xrt6xclbinC2ERKSt17basic_string_viewIcSt11char_traitsIcEE",
  "xrt::xclbin::xclbin(const std::vector<char>&)", "_ZN3xrt6xclbinC2ERKSt6vectorIcSaIcEE",
  "xrt::xclbin_repository::begin(void)", "_ZNK3xrt17xclbin_repository5beginEv",
  "xrt::xclbin_repository::end(void)", "_ZNK3xrt17xclbin_repository3endEv",
  "xrt::xclbin_repository::iterator::iterator(const xrt::xclbin_repository::iterator&)", "_ZN3xrt17xclbin_repository8iteratorC2ERKS1_",
  "xrt::xclbin_repository::iterator::operator*(void)", "_ZNK3xrt17xclbin_repository8iteratordeEv",
  "xrt::xclbin_repository::iterator::operator++(int)", "_ZN3xrt17xclbin_repository8iteratorppEi",
  "xrt::xclbin_repository::iterator::operator++(void)", "_ZN3xrt17xclbin_repository8iteratorppEv",
  "xrt::xclbin_repository::iterator::operator->(void)", "_ZNK3xrt17xclbin_repository8iteratorptEv",
  "xrt::xclbin_repository::iterator::operator==(const xrt::xclbin_repository::iterator&)", "_ZNK3xrt17xclbin_repository8iteratoreqERKS1_",
  "xrt::xclbin_repository::iterator::path(void)", "_ZNK3xrt17xclbin_repository8iterator4pathB5cxx11Ev",
  "xrt::xclbin_repository::load(const std::string&)", "_ZNK3xrt17xclbin_repository4loadERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin_repository::xclbin_repository(const std::string&)", "_ZN3xrt17xclbin_repositoryC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
  "xrt::xclbin_repository::xclbin_repository(void)", "_ZN3xrt17xclbin_repositoryC2Ev",
};
};

size_t
get_size_of_func_mangled_map()
{
  return sizeof(func_mangled_map)/sizeof(func_mangled_map[0]);
}
#endif
