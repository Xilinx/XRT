/**
* Copyright (C) 2019-2022 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

#include "include.h"
#include "pl_controller.hpp"

// XRT includes
#include "experimental/xrt_system.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

namespace po = boost::program_options;

int
main(int argc, char* argv[])
{
    std::string b_file = "/vck5000_pcie_pl_controller.xclbin.xclbin";

    // Option Variables
    std::string test_path;
    std::string dev_id = "0";
    bool flag_s = false;

    // -- Retrieve and parse the subcommand options
    // -----------------------------

    po::options_description desc("Available Options");
    desc.add_options()(
        "path,p", boost::program_options::value<decltype(test_path)>(&test_path)
                      ->required(),
        "Platform test path")(
        "device,d", boost::program_options::value<decltype(dev_id)>(&dev_id),
        "Device ID")("supported,s",
                     boost::program_options::bool_switch(&flag_s),
                     "Supported")("help,h", "Prints this help menu.");

    po::positional_options_description positionals;

    // Parse sub-command ...
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(positionals)
                      .run(),
                  vm);
        // Print out the help menu
        if (vm.count("help") != 0) {
            std::cout << desc << "\n";
            return EXIT_SUCCESS;
        }

        // Validate the options
        po::notify(vm); // Can throw
    } catch (po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cout << desc << "\n";
        return EXIT_FAILURE;
    }

    std::string binaryfile = test_path + b_file;
    std::ifstream infile(binaryfile);

    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    }

    if (flag_s) {
        std::cout << "\nSUPPORTED" << std::endl;
        return EXIT_SUCCESS;
    }

    auto num_devices = xrt::system::enumerate_devices();
    auto device = xrt::device{ dev_id };
    auto uuid = device.load_xclbin(binaryfile);

    // instance of plController
    std::string aie_control_file = "/aie_control_config.json";
    std::string dma_lock_file = "/dma_lock_report.json";
    std::string aie_control = test_path + aie_control_file;
    std::string dma_lock = test_path + dma_lock_file;
    xf::plctrl::plController m_pl_ctrl(aie_control.c_str(), dma_lock.c_str());

    int num_iter = 2;
    int num_sample = 16;

    m_pl_ctrl.enqueue_update_aie_rtp("mygraph.first.in[1]", num_sample);
    m_pl_ctrl.enqueue_sleep(128);
    m_pl_ctrl.enqueue_set_aie_iteration("mygraph", num_iter);
    m_pl_ctrl.enqueue_enable_aie_cores();

    m_pl_ctrl.enqueue_loop_begin(num_iter/2);
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 0,
                                             num_sample);
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 0,
                                             num_sample);
    m_pl_ctrl.enqueue_sync(num_sample);
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 1,
                                             num_sample);
    m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 1,
                                             num_sample);
    m_pl_ctrl.enqueue_sync(num_sample);
    if (num_iter%2 != 0) {
	m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.in[0]", 0,
						 num_sample);
	m_pl_ctrl.enqueue_set_and_enqueue_dma_bd("mygraph.first.out[0]", 0,
						 num_sample);
	m_pl_ctrl.enqueue_sync(num_sample);
    }
    m_pl_ctrl.enqueue_loop_end();
    
    m_pl_ctrl.enqueue_sleep(128);
    m_pl_ctrl.enqueue_disable_aie_cores();
    m_pl_ctrl.enqueue_halt();

    unsigned int mem_size = 0;

    auto sender_receiver_k1 =
        xrt::kernel(device, uuid, "sender_receiver:{sender_receiver_1}");
    auto controller_k1 =
        xrt::kernel(device, uuid, "pl_controller_kernel:{controller_1}");

    // output memory
    mem_size = num_sample * num_iter * sizeof(int);
    auto out_bo1 = xrt::bo(device, mem_size, sender_receiver_k1.group_id(2));
    auto host_out1 = out_bo1.map<int*>();

    // input memory
    auto in_bo1 = xrt::bo(device, mem_size, sender_receiver_k1.group_id(2));
    auto host_in1 = in_bo1.map<int*>();

    std::cout << " memory allocation complete" << std::endl;

    // initialize input memory
    for (unsigned int i = 0; i < mem_size / sizeof(int); i++)
        *(host_in1 + i) = i;

    in_bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, mem_size, /*OFFSET=*/0);

    int32_t num_pm = m_pl_ctrl.get_microcode_size(); /// sizeof(int32_t);
    auto pm_bo = xrt::bo(device, (num_pm + 1) * sizeof(uint32_t),
                         controller_k1.group_id(4));
    auto host_pm = pm_bo.map<uint32_t*>();

    m_pl_ctrl.copy_to_device_buff(host_pm + 1);
    host_pm[0] = num_pm;

    // sync input memory for pl_controller
    pm_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, (num_pm + 1) * sizeof(uint32_t),
               /*OFFSET=*/0);
    std::cout << "sync pm buffer complete" << std::endl;

    // start pl controller
    int ctrl_pkt_id = 0;
    auto controller_r1 = xrt::run(controller_k1);
    controller_r1.set_arg(3, ctrl_pkt_id);
    controller_r1.set_arg(4, pm_bo);
    controller_r1.start();
    std::cout << "start pl controller kernel" << std::endl;
    // start input kernels

    // start sender_receiver kernels
    auto sender_receiver_r1 = xrt::run(sender_receiver_k1);
    sender_receiver_r1.set_arg(0, num_iter);
    sender_receiver_r1.set_arg(1, in_bo1);
    sender_receiver_r1.set_arg(2, out_bo1);
    sender_receiver_r1.start();
    std::cout << " start sender-receiver kernel" << std::endl;

    controller_r1.wait();
    std::cout << " pl controller wait complete" << std::endl;
    sender_receiver_r1.wait();
    std::cout << " sender_receiver wait complete" << std::endl;

    // sync output memory
    out_bo1.sync(XCL_BO_SYNC_BO_FROM_DEVICE, mem_size, /*OFFSET=*/0);

    // post-processing data;
    bool match = 0;
    for (unsigned int i = 0; i < mem_size / sizeof(int); i++) {
        if (*(host_out1 + i) != *(host_in1 + i) + 1) {
            match = 1;
            std::cout << boost::format("host_out1[%u]=%d\n") % i % host_out1[i];
        }
    }

    // report and return PASS / FAIL status
    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl;
    return (match ? EXIT_FAILURE : EXIT_SUCCESS);
}
