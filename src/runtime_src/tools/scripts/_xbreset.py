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

#path of the serial_num sysfs node of FPGA user PF
#serial num is being used to accurately identify which 2 FPGAs are on same card
serial_num = {}

rootDir = "/sys/bus/pci/devices/"

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
                break
            if re.search("^xmc\.u.+$", fname) != None:
                serial_num[subdirName] = os.path.join(rootDir, subdirName, fname, "serial_num")
                #print("%s: %s " % (subdirName, serial_num[subdirName]))
                break
        if not os.path.exists(os.path.join(rootDir, subdirName, "dparent")):
            continue
        files = os.listdir(os.path.join(rootDir, subdirName, "dparent"))
        for fname in files:
            if re.search("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}.[0-7]:pcie.+$", fname) != None:
            	#print("\tfile: %s" % fname)
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
    mgmt = user[:len(user)-1] + "0"
    nodes_in_cards = bus_children[node_bus_mapping[mgmt]]
    for node in nodes_in_cards:
        buddy_user = node[:len(node)-1] + "1"
        with open(serial_num[buddy_user]) as f : buddy_sn = f.read()
        if buddy_user != user and sn == buddy_sn:
            return buddy_user
    return None

#do pcie node remove and rescan
def run(cmdline):
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
        This cmdline tool is mainly used to do card level reset.
        Eg. U30 card which has 2 FPGAs in one card. When whichever FPGA is
        specified, both FPGAs on the card will be reset.

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

        #print(node_parent_mapping)
        buddy_user = get_buddy(user)
        print("Card level reset. This will reset all FPGAs on the card")
        print("All existing processes will be killed.")
        while not args.yes:
            line = input("Are you sure you wish to proceed? [y/n]:")
            if line == "n":
                sys.exit(1)
            if line == "y":
                break
        #steps to do card level reset
        #1. kill processes running on both cards
        #2. remove FPGA that SBR is not being issued on
        #3. trigger SBR on specified FPGA
        #4. remove nodes of specified FPGA
        #5. rescan pcie 
        #6. wait back online

        #1
        print("shutdown: %s" % user)
        run("echo 2 > " + os.path.join(rootDir, user, "shutdown"))
        #2
        if buddy_user:
            print("shutdown: %s" % buddy_user)
            run("echo 2 > " + os.path.join(rootDir, buddy_user, "shutdown"))
            buddy_mgmt = buddy_user[:len(buddy_user)-1] + "0"
            print("remove: %s, %s" % (buddy_user, buddy_mgmt))
            run("echo 1 > " + os.path.join(rootDir, buddy_user, "remove"))
            run("echo 1 > " + os.path.join(rootDir, buddy_mgmt, "remove"))
        #3
        print("SBR reset...")
        run("echo 1 > " + os.path.join(rootDir, mgmt, "sbr_toggle"))
        time.sleep(3)
        #4
        print("remove: %s, %s" % (user, mgmt))
        run("echo 1 > " + os.path.join(rootDir, user, "remove"))
        run("echo 1 > " + os.path.join(rootDir, mgmt, "remove"))
        #5
        print("rescan pci")
        run("echo 1 > /sys/bus/pci/rescan")
        time.sleep(1)
        #6
        print("wait FPGAs back on line")
        wait_ps_online(mgmt)
        wait_xocl_online(user)
        if buddy_user:
            buddy_mgmt = buddy_user[:len(buddy_user)-1] + "0"
            wait_ps_online(buddy_mgmt)
            wait_xocl_online(buddy_user)

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
