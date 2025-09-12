// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#include <sstream>

#include "npu3_transaction.h"
#include "core/common/message.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/experimental/xrt_ext.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

#include "core/common/aiebu/src/cpp/include/aiebu/aiebu_assembler.h"
#include "core/common/aiebu/src/cpp/include/aiebu/aiebu_error.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>

extern "C" {
    #include <xaiengine.h>
    #include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie {
    using severity_level = xrt_core::message::severity_level;

    bool NPU3Transaction::initializeTransaction(XAie_DevInst* aieDevInst, std::string tName) 
    {
        setTransactionName(tName);
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
            "Writing to New Control Code ASM file: " + getAsmFileName());

        try {
            // NOTE: XAIE_IO_BACKEND_CONTROLCODE is the default
            // XAie_SetIOBackend(aieDevInst, XAIE_IO_BACKEND_CONTROLCODE);
            XAie_OpenControlCodeFile(aieDevInst, getAsmFileName().c_str(), 8192);
            XAie_StartNewJob(aieDevInst);
            return true;
        }
        catch(const std::exception& e) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT",
                "Error in generating asm File: " + getAsmFileName() + "\n" + e.what());
        }
        xrt_core::message::send(severity_level::warning, "XRT", "AIE Transaction Initialization Failed.");
        return false;
    }

    bool NPU3Transaction::completeASM(XAie_DevInst* aieDevInst)
    {
        //
        // 1. End generation of ASM file
        //
        try {
            XAie_EndJob(aieDevInst);
            XAie_EndPage(aieDevInst);
            XAie_CloseControlCodeFile(aieDevInst);
        }
        catch(const std::exception& e) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT",
                "Error in generating ASM file: " + getAsmFileName() + "\n" + e.what());
            return false;
        }
        return true;
    }

    bool NPU3Transaction::generateELF() 
    {
        //
        // 2. Convert ASM to ELF
        //
        // Fill this vector with ASM content
        std::vector<char> control_code_buf;
        std::vector<std::string> libpaths;
        libpaths.push_back("./");

        try {
#if 1
            //Read ASM file
            std::string asmFileName = getAsmFileName();
            if (!std::filesystem::exists(asmFileName))
                throw std::runtime_error("file:" + asmFileName + " not found\n");

            std::ifstream inAsm(asmFileName, std::ios::in | std::ios::binary);
            std::cout << "Open file " << asmFileName << std::endl;

            auto file_size = std::filesystem::file_size(asmFileName);
            control_code_buf.resize(file_size);

            inAsm.read(control_code_buf.data(), file_size);
            std::streamsize bytesRead = inAsm.gcount();
            if (static_cast<std::size_t>(bytesRead) != static_cast<std::size_t>(file_size)) {
                std::cerr << "Read " << bytesRead << " bytes but expected " << file_size
                                            << " for file " << asmFileName << '\n';
                control_code_buf.resize(static_cast<std::size_t>(bytesRead)); // keep only read bytes
            } else {
                std::cout << "ASM file read (" << file_size << " bytes): " << asmFileName << '\n';
            }

            //Convert ASM to ELF data.
            auto as = aiebu::aiebu_assembler(aiebu::aiebu_assembler::buffer_type::asm_aie4,
                                             control_code_buf, {},  libpaths);
            
            //Write elf data to a file
            auto e = as.get_elf();
            std::cout << "Elf size:" << e.size() << std::endl;
            std::ofstream outElf(getElfFileName(), std::ios_base::binary);
            outElf.write(e.data(), e.size());
#else
            auto check1 = std::getenv("AIEBU_REPO");
            auto check2 = std::getenv("PYTHONPATH");
            if ((check1 == nullptr) || (check2 == nullptr)) {
                xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                  "Please define AIEBU_REPO and PYTHONPATH so elf generation can work.");
                return false;
            }

            std::stringstream command;
            command << "${AIEBU_REPO}/src/python/aiebu/control_asm_disasm.py -t aie4 "
                    << getAsmFileName() << " -o " << getElfFileName();
            xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                                    "Generating ELF using: " + command.str());
            if (system(command.str().c_str())) {
                xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
                                        "Elf generation failed");
                return false;
            }
#endif
        }
        catch(const std::exception& e) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT",
                "Error in generating Elf file: " + getElfFileName() + "\n" + e.what());
            return false;
        }
        return true;
    }

    bool NPU3Transaction::submitELF(xrt::hw_context hwContext) 
    {
        //
        // 3. Submit ELF to microcontroller
        //
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
            "Start New Control Code Elf");
        xrt::elf profileElf;
        try {
            profileElf = xrt::elf(getElfFileName());
        } 
        catch (...) {
            xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
            "Failed to load " + getElfFileName() + ". Cannot configure AIE to profile.");
            return false;
        }

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Elf Object Created");
        xrt::module mod{profileElf};

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Module Created");
        xrt::kernel kernel;
        try {
            kernel = xrt::ext::kernel{hwContext, mod, "XDP_KERNEL:{IPUV1CNN}"};
        } catch (...) {
            xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
            "XDP_KERNEL not found in HW Context. Unable to run " + getElfFileName());
            return false;
        }

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "XDP_KERNEL created");
        xrt::run run{kernel};

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Kernel run created");
        run.start();

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Run started");
        run.wait2();

        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Wait done!");
        return true;
    }

    bool NPU3Transaction::submitTransaction(XAie_DevInst* aieDevInst, xrt::hw_context hwContext) 
    {
        if (!completeASM(aieDevInst))
            return false;
        if (!generateELF())
            return false;
        if (!submitELF(hwContext))
            return false;
        return true;
    }
}
