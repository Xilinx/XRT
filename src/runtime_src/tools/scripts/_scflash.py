#!/usr/bin/env python3
"""
 Copyright (C) 2020-2021 Xilinx, Inc
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

#path of the serial_num sysfs node of FPGA user PF
#serial num is being used to accurately identify which 2 FPGAs are on same card
serial_num = {}

#path of the sc_is_fixed sysfs node of the FPGA
sc_is_fixed = {}

rootDir = "/sys/bus/pci/devices/"
xbmgmt = "/opt/xilinx/xrt/bin/unwrapped/xbmgmt"

#get all above "node vs bus" mappings of xilinx FPGAs
def get_node_bus_mapping():
    subdir = os.listdir(rootDir)
    for subdirName in subdir:
        if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[01]$", subdirName) == None:
            continue
        with open(os.path.join(rootDir, subdirName, "vendor")) as f : vendor = f.read()
        if vendor.strip() != "0x10ee":
            continue
        files = os.listdir(os.path.join(rootDir, subdirName))
        for fname in files:
            if re.search("^processor_system.+$", fname) != None:
                ps_ready[subdirName] = os.path.join(rootDir, subdirName, fname, "ps_ready")
                #print("%s: %s " % (subdirName, ps_ready[subdirName]))
            if re.search("^xmc\.m.+$", fname) != None:
                sc_is_fixed[subdirName] = os.path.join(rootDir, subdirName, fname, "sc_is_fixed")
                #print("%s: %s " % (subdirName, sc_is_fixed[subdirName]))
            if re.search("^xmc\.u.+$", fname) != None:
                serial_num[subdirName] = os.path.join(rootDir, subdirName, fname, "serial_num")
                #print("%s: %s " % (subdirName, serial_num[subdirName]))
                break
        if not os.path.exists(os.path.join(rootDir, subdirName, "dparent")):
            continue
        files = os.listdir(os.path.join(rootDir, subdirName, "dparent"))
        for fname in files:
            if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-7]:pcie.+$", fname) != None:
                bus = fname.split(":")[1]
                node_bus_mapping[subdirName] = bus
                node_parent_mapping[subdirName] = fname[:fname.rfind(":")]
                if bus not in bus_children:
                    bus_children[bus] = []
                bus_children[bus].append(subdirName);

#get buddy FPGA on same card
def get_buddy(user):
    #print(serial_num)
    with open(serial_num[user]) as f : sn = f.read()

    if sn == "":
        return None

    mgmt = user[:len(user)-1] + "0"
    nodes_in_cards = bus_children[node_bus_mapping[mgmt]]
    for node in nodes_in_cards:
        buddy_user = node[:len(node)-1] + "1"
        with open(serial_num[buddy_user]) as f : buddy_sn = f.read()
        if buddy_user != user and sn == buddy_sn:
            return buddy_user
    return None

#call 'xbmgmt' to do the real flash
def run_xbmgmt(cmdline):
    print(cmdline)
    p = subprocess.Popen(cmdline, stdout=subprocess.PIPE)
    out, err = p.communicate(timeout=120)
    if out:
        print(out.decode("utf-8"))
    if err:
        print(err.decode("utf-8"))
    return p.returncode

#do pcie node remove and rescan
def run_pcie(cmdline):
    #print(cmdline)
    subprocess.call(cmdline, shell=True)

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

        with open(sc_is_fixed[mgmt]) as f : fixed = f.read()
        if fixed.strip() == "1":
            print("ERROR: %s has fixed sc!" % mgmt)
            sys.exit(1)

        #print(node_parent_mapping)
        buddy_user = get_buddy(user)
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
        #1. kill processes running on the both FPGAs
        #Note: since get FPGAs on same card relies on the S/N info, which in turn
        #      relies on a working SC, so it is possible that we can't get S/N info
        #      and hence get FPGA pair on same card before flashing. In this case,
        #      we just don't try to kill processes running on the buddy FPGA. This
        #      should be fine
        #2. call 'xbmgmt flash' on the specified FPGA
        #3. call 'xbmgmt reset --ert' on the 'fixed sc' FPGA to reboot cmc
        #   this step 3 is a workaround to https://jira.xilinx.com/browse/CR-1076187
        #4. wait for FPGAs back online
        # steps 3 & 4 are not needed anymore since CR-1076187 has been fixed. this
        # will save some time for the sc flash
        #Note: since this python is called by xbmgmt, so we can't do a card reset
        #      here, otherwise, card reset will remove the xclmgmt which is still
        #      being used by the calling xbmgmt.
        #      The other reason we don't do a card reset before sc flash is, the
        #      (new) card reset solution rely on a working SC to report S/N info, so
        #      there is a chicken-and-egg problem here

        #1
        print("shutdown: %s" % user)
        run_pcie("echo 2 > " + os.path.join(rootDir, user, "shutdown"))
        if buddy_user:
            print("shutdown: %s" % buddy_user)
            run_pcie("echo 2 > " + os.path.join(rootDir, buddy_user, "shutdown"))
        #2
        print("sc flash...")
        retcode = run_xbmgmt([xbmgmt, "flash", "--sc_firmware", "--path", args.path, "--card", mgmt, "--no_cardlevel"])
        return retcode
    except Exception as e:
        print(e)

if __name__ == "__main__":
    if main() == 0:
        print("SC flash PASSED")
        exit(0)
    exit(1)
