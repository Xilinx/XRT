#!/usr/bin/env python3
"""
 Copyright (C) 2020 Xilinx, Inc
 Author(s): Brian Xu (brianx@xlinx.com)

 Licensed under the Apache License, Version 2.0 (the "License"). You may
 not use this file except in compliance with the License. A copy of the
 License is located at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 License for the specific language governing permissions and limitations
 under the License.
"""

import sys
import argparse
import textwrap
import os
import re
import subprocess
import time

#In this 'lspci -t' snippet
#-+-[0000:16]-+-00.0-[17]--+-00.0
# |           |            \-00.1
# |           +-01.0-[18]--+-00.0
# |           |            \-00.1
#
#FPGA 17:00.[01] parent is 16.00.0, which has primary bus number 16, and secondary bus number 17
#FPGA 18:00.[01] parent is 16.01.0, which has primary bus number 16, and secondary bus number 18
#FPGAs whose parents have identical pcie primary bus number are on same card

#FPGA node and its parent primary bus number mapping
#eg "0000:17:00.0" "16"
#   "0000:18:00.0" "16"
node_bus_mapping = {}
#FPGA node and its parent node DBDF mapping
#eg "0000:17:00.0" "0000:16:00.0"
#   "0000:18:00.0" "0000:16:01.0"
node_parent_mapping = {}
#primary bus of pcie bridge and FPGA node mapping
#eg "16" ["0000:17.00.0", "0000:18:00.0"]
bus_children = {}

#path of the ps_ready sysfs node of the FPGA
ps_ready = {}

#path of the sc_is_fixed sysfs node of the FPGA
sc_is_fixed = {}

rootDir = "/sys/bus/pci/devices/"
xbmgmt = "/opt/xilinx/xrt/bin/unwrapped/xbmgmt"

#get all above "node vs bus" mappings of xilinx FPGAs
def get_node_bus_mapping():
    subdir = os.listdir(rootDir)
    for subdirName in subdir:
        if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.0$", subdirName) == None:
            continue
        with open(os.path.join(rootDir, subdirName, "vendor")) as f : vendor = f.read()
        if vendor.strip() != "0x10ee":
            continue
        files = os.listdir(os.path.join(rootDir, subdirName))
        for fname in files:
            if re.search("^processor_system.+$", fname) != None:
                ps_ready[subdirName] = os.path.join(rootDir, subdirName, fname, "ps_ready")
            if re.search("^xmc.+$", fname) != None:
                sc_is_fixed[subdirName] = os.path.join(rootDir, subdirName, fname, "sc_is_fixed")
        #print("%s: %s " % (subdirName, ps_ready[subdirName]))
        files = os.listdir(os.path.join(rootDir, subdirName, "dparent"))
        for fname in files:
            if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-7]:pcie.+$", fname) != None:
                bus = fname.split(":")[1]
                node_bus_mapping[subdirName] = bus
                node_parent_mapping[subdirName] = fname[:fname.rfind(":")]
                if bus not in bus_children:
                    bus_children[bus] = []
                bus_children[bus].append(subdirName);

#call 'xbmgmt' to do the real flash
def run_xbmgmt(cmdline):
    print(cmdline)
    p = subprocess.Popen(cmdline, stdout=subprocess.PIPE)
    out, err = p.communicate(timeout=120)
    if out:
        print(out.decode("utf-8"))
    if err:
        print(err.decode("utf-8"))

#call 'xbmgmt reset'
def run_reset(cmdline):
    print(cmdline)
    p1 = subprocess.Popen(["echo", "y"], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stdin=p1.stdout)
    p1.stdout.close()
    p2.communicate(timeout=90)[0]

#do pcie node remove and rescan
def run_pcie(cmdline):
    #print(cmdline)
    subprocess.call(cmdline, shell=True)

#wait ps on FPGA back online
def wait_ps_online(mgmt):
    while True:
        with open(ps_ready[mgmt]) as f : ready = f.read()
        if ready.strip() == '0':
            time.sleep(3)
            continue
        print("%s ps ready" % mgmt)
        break

#wait FPGA back online
def wait_xocl_online(user):
    while True:
        with open(os.path.join(rootDir, user, "dev_offline")) as f : ready = f.read()
        if ready.strip() == '1':
            time.sleep(1)
            continue
        print("%s host online" % user)
        break


def main():
    desc = textwrap.dedent('''\
        This cmdline tool is mainly used to flash sc on U30.
        U30 card has 2 FPGAs in one card. Each FPGA has one cmc running
        on the petalinux but there is only one sc. Only one of the cmc
        can be used to flash the sc. Please run 'xbmgmt flash --scan' to
        find the one *without* "Fixed"
        If there is only one FPGA node in the card, the cmd will fall back
        to 'xbmgmt flash --sc_firmware' expert cmd

        Root privilege is required to run this cmd
        
        Example:
            scflash -d 0000:17:00.0 -p path_to_sc_firmware
    ''')
    if (os.geteuid() != 0):
        print("ERROR: root privilege is required.")
        sys.exit(1)

    parser=argparse.ArgumentParser(description=desc,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-d","--dbdf", required=True,
        help="DBDF of a FPGA node, with format xxxx:xx:xx.x")
    parser.add_argument("-p","--path", required=True,
        help="path to the sc firmware file")
    parser.add_argument("-y","--yes", help="automatic yes to prompts",
        action="store_true")
    args = parser.parse_args()

    get_node_bus_mapping()
    mgmt = args.dbdf[:len(args.dbdf)-1] + "0"
    user = args.dbdf[:len(args.dbdf)-1] + "1"

    try:
        if mgmt not in node_bus_mapping:
            print("ERROR: Is %s a FPGA node?" % user)
            sys.exit(1)

        #print(node_parent_mapping)
        nodes_in_cards = bus_children[node_bus_mapping[mgmt]]
        #print(nodes_in_cards)
        print("This will reflash sc on the card")
        print("All existing processes will be killed.")
        while not args.yes:
            line = input("Are you sure you wish to proceed? [y/n]:")
            if line == "n":
                sys.exit(1)
            if line == "y":
                break
        #steps to flash sc
        #1. kill processes running on the other FPGA
        #2. call 'xbmgmt flash' on the specified FPGA
        #3. call 'xbmgmt reset --ert' on the 'fixed sc' FPGA to reboot cmc
        #   this step 3 is a workaround to https://jira.xilinx.com/browse/CR-1076187
        #4. wait for FPGAs back online
        #Note: since this python is called by xbmgmt, so we can't do a card reset
        #      here, otherwise, card reset will remove the xclmgmt which is still
        #      being used by the calling xbmgmt.
        for node in nodes_in_cards:
            user = node[:len(node)-1] + "1"
            print("shutdown: %s" % user)
            run_pcie("echo 2 > " + os.path.join(rootDir, user, "shutdown"))
        print("sc flash...")
        run_xbmgmt([xbmgmt, "flash", "--sc_firmware", "--path", args.path, "--card", mgmt, "--no_cardlevel"])
        time.sleep(1)
        print("ERT reset...")
        for node in nodes_in_cards:
            with open(sc_is_fixed[node]) as f : fixed_sc = f.read()
            if fixed_sc.strip() != "1":
                continue
            run_reset([xbmgmt, "reset", "--ert", "--card", node])
        time.sleep(1)
        print("wait FPGAs back on line")
        for node in nodes_in_cards:
            user = node[:len(node)-1] + "1"
            wait_ps_online(node)
            wait_xocl_online(user)
    except Exception as e:
        print(e)

if __name__ == "__main__":
    main()
