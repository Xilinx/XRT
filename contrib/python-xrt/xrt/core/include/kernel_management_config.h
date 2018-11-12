#ifndef __KERNEL_MANAGEMENT_CONSTANT__
#define __KERNEL_MANAGEMENT_CONSTANT__

class Kernel_control_config {
public:
    virtual unsigned get_ap_control() = 0;
    virtual unsigned get_base_arg() = 0;
    virtual unsigned get_64arch_arg_size() = 0;
    virtual unsigned get_32arch_arg_size() = 0;
    virtual ~Kernel_control_config() {}
};

class Alevo_kernel_control_config : public Kernel_control_config {
public:
    unsigned get_ap_control() {return 0x0;}
    unsigned get_base_arg() {return 0x10;}
    unsigned get_64arch_arg_size() {return 0xc;}
    unsigned get_32arch_arg_size() {return 0x8;}
};

#endif