#include "kernel_management_api.h"

void configure_ert(string device_name, unsigned buffer_handle, py::dict config)
{
    void *exec_data = device_dict[device_name]->buffer_dict[buffer_handle].addr;

    auto ert_command = reinterpret_cast<ert_configure_cmd *>(exec_data);
    memset(ert_command, 0, device_dict[device_name]->buffer_dict[buffer_handle].size);
    unsigned slot_size = py::extract<unsigned>(config["slot_size"]);
    unsigned num_compute_unit = py::extract<unsigned>(config["num_compute_unit"]);
    unsigned compute_unit_shift = py::extract<unsigned>(config["compute_unit_shift"]);
    unsigned compute_unit_base_addr = py::extract<unsigned>(config["compute_unit_base_addr"]);
    bool enable_ert = py::extract<bool>(config["enable_ert"]);
    unsigned compute_unit_dma = py::extract<unsigned>(config["compute_unit_dma"]);
    unsigned compute_unit_isr = py::extract<unsigned>(config["compute_unit_isr"]);
    ert_command->state = convert_ert_command_state(config["state"]);
    ert_command->opcode = convert_ert_command_opcode(config["opcode"]);
    ert_command->slot_size = slot_size;
    ert_command->num_cus = num_compute_unit;
    ert_command->cu_shift = compute_unit_shift;
    ert_command->cu_base_addr = compute_unit_base_addr;
    ert_command->ert = enable_ert;
    if (enable_ert)
    {
        ert_command->cu_dma = compute_unit_dma;
        ert_command->cu_isr = compute_unit_isr;
    }
    ert_command->data[0] = compute_unit_base_addr;
    ert_command->count = 5 + ert_command->num_cus;
    cout << "Set ert at kernel base address 0x" << hex << compute_unit_base_addr << endl;
}

void start_kernel(string device_name, unsigned buffer_handle, py::dict command)
{
    void *command_data = device_dict[device_name]->buffer_dict[buffer_handle].addr;
    auto start_command = reinterpret_cast<ert_start_kernel_cmd *>(command_data);
    long num_args = py::len(command["argument_addr"]);
    vector<unsigned long> arg_value_list;
    for (unsigned arg_idx = 0; arg_idx < num_args; ++arg_idx)
    {
        unsigned long arg_addr = py::extract<unsigned long>(command["argument_addr"][arg_idx]);
        arg_value_list.push_back(arg_addr);
    }
    bool is_64bit = is_64bit_arch(arg_value_list);
    unsigned compute_unit_mask = py::extract<unsigned>(command["compute_unit_mask"]);
    Kernel_control_config *config = new Alevo_kernel_control_config();
    memset(start_command, 0, device_dict[device_name]->buffer_dict[buffer_handle].size);
    start_command->state = convert_ert_command_state(command["state"]);
    start_command->opcode = convert_ert_command_opcode(command["opcode"]);
    start_command->cu_mask = compute_unit_mask;
    unsigned long ap_control_addr = config->get_ap_control();
    unsigned long current_base_arg_addr = config->get_base_arg();
    unsigned long arg_size = is_64bit ? config->get_64arch_arg_size() : config->get_32arch_arg_size();
    start_command->data[ap_control_addr] = 0x0;
    for (unsigned arg_idx = 0; arg_idx < num_args; ++arg_idx)
    {
        unsigned long full_addr = arg_value_list[arg_idx];
        unsigned long arg_value_low = full_addr & 0xFFFFFFFF;
        unsigned long arg_value_high = (full_addr >> 32) & 0xFFFFFFFF;
        cout << "Setting kernel argument address 0x" << hex << full_addr << " at 0x" << current_base_arg_addr << dec << endl;
        start_command->data[current_base_arg_addr / 4] = arg_value_low;
        if (is_64bit) {
            start_command->data[current_base_arg_addr / 4 + 1] = arg_value_high;
        }
        current_base_arg_addr = current_base_arg_addr + arg_size;
    }
    unsigned payload_size = current_base_arg_addr / 4;
    start_command->count = payload_size;
    delete config;
}

#if !defined(SW_EMU)

void execute_wait(string device_name, int timeout)
{
    while (xclExecWait(device_dict[device_name]->handle, timeout) == 0)
    {
        cout << "Waiting for buffer execution ..." << endl;
    }
    return;
}

void execute_buffer(string device_name, unsigned buffer_handle)
{
    auto err = xclExecBuf(device_dict[device_name]->handle, buffer_handle);
    if (err)
    {
        cout << "Error code: " << err << endl;
        throw runtime_error("Unable to issue execute buffer");
    }
}

#endif