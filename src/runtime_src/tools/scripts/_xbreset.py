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

rootDir = "/sys/bus/pci/devices/"

#get all above "node vs bus" mappings of xilinx FPGAs 
def get_node_bus_mapping():
    subdir = os.listdir(rootDir)
    for subdirName in subdir:
        if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-3]{2}.0$", subdirName) == None:
            continue
        with open(os.path.join(rootDir, subdirName, "vendor")) as f : vendor = f.read()
        if vendor.strip() != "0x10ee":
            continue
        #print("node: %s" % subdirName)
        files = os.listdir(os.path.join(rootDir, subdirName, "dparent"))
        for fname in files:
            if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-3]{2}.[0-7]:pcie.+$", fname) != None:
            	#print("\tfile: %s" % fname)
                bus = fname.split(":")[1]
                node_bus_mapping[subdirName] = bus
                node_parent_mapping[subdirName] = fname[:fname.rfind(":")]
                if bus not in bus_children:
                    bus_children[bus] = []
                bus_children[bus].append(subdirName);

#call 'xbutil reset' to do the real reset
def run_reset(cmdline):
    #print cmdline
    p1 = subprocess.Popen(["echo", "y"], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stdin=p1.stdout)
    p1.stdout.close()
    p2.communicate(timeout=60)[0]

#do pcie node remove and rescan
def run(cmdline):
    #print cmdline
    subprocess.call(cmdline, shell=True)

def main():
    desc = textwrap.dedent('''\
        This cmdline tool is mainly used to do card level reset.
        Eg. U30 card which has 2 FPGAs in one card. When whichever FPGA is
        specified, both FPGAs on the card will be reset.
        If there is only one FPGA node in the card, the cmd will fall back
        to 'xbutil reset'

        Root privilege is required to run this cmd
        
        Example:
            xbrest -d 0000:17:0.1
    ''')
    if (os.geteuid() != 0):
        print("ERROR: root privilege is required.")
        sys.exit(1)

    parser=argparse.ArgumentParser(description=desc,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-d","--dbdf", required=True,
        help="DBDF of a FPGA node, with format xxxx:xx:xx.x")
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
        with open(os.path.join(rootDir, user, "ready")) as f : ready = f.read()
        ready = ready.strip("\n")
        if ready == "0x0" or ready == "0":
            print("ERROR: FPGA %s is not usable" % user)
            sys.exit(1)

        #print node_parent_mapping
        nodes_in_cards = bus_children[node_bus_mapping[mgmt]]
        #print nodes_in_cards
        if len(nodes_in_cards) == 1:
            #print("xbutil reset %s" % user)
            print("All existing processes will be killed.")
            while not args.yes:
                line = input("Are you sure you wish to proceed? [y/n]:")
                if line == "n":
                    sys.exit(1)
                if line == "y":
                    break
            run_reset(["/opt/xilinx/xrt/bin/unwrapped/xbutil", "reset", "-d", user])
        elif (len(nodes_in_cards) == 2):
            if mgmt == nodes_in_cards[0]:
                rm = nodes_in_cards[1]
            else:
                rm = nodes_in_cards[0]
            print("Card level reset. This will reset all FPGAs on the card: %s, %s" % (mgmt, rm))
            print("All existing processes will be killed.")
            while not args.yes:
                line = input("Are you sure you wish to proceed? [y/n]:")
                if line == "n":
                    sys.exit(1)
                if line == "y":
                    break
            rm_user = rm[:len(rm)-1] + "1"
            #steps to do card level reset
            #1. kill processes running on one card
            #2. remove nodes of that FPGA
            #3. call 'xbutil reset' on another FPGA
            #4. rescan pcie 
            print("shutdown: %s" % rm_user)
            run("echo 2 > " + os.path.join(rootDir, rm_user, "shutdown"))
            print("remove: %s, %s" % (rm_user, rm))
            run("echo 1 > " + os.path.join(rootDir, node_parent_mapping[rm], "remove"))
            print("xbutil reset %s" % user)
            run_reset(["/opt/xilinx/xrt/bin/unwrapped/xbutil", "reset", "-a", "-d", user])
            print("rescan pci")
            run("echo 1 > /sys/bus/pci/rescan")
    except (subprocess.TimeoutExpired):
        #xbutil reset timeout, try remove it
        print("shutdown: %s" % user)
        run("echo 2 > " + os.path.join(rootDir, user, "shutdown"))
        print("remove: %s, %s" % (user, mgmt))
        run("echo 1 > " + os.path.join(rootDir, node_parent_mapping[mgmt], "remove"))
        print("rescan pci")
        run("echo 1 > /sys/bus/pci/rescan")
    except Exception as e:
        print(e)

if __name__ == "__main__":
    main()
