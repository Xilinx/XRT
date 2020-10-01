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
import getopt
import os
import re
import subprocess
import time

node_bus_mapping = {}
node_parent_mapping = {}
bus_children = {}
rootDir = "/sys/bus/pci/devices/"

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

def run_reset(cmdline):
    #print cmdline
    p1 = subprocess.Popen(["echo", "y"], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stdin=p1.stdout)
    p1.stdout.close()
    p2.communicate(timeout=60)[0]

def run(cmdline):
    #print cmdline
    subprocess.call(cmdline, shell=True)

def usage():
    print("Usage:")
    print("\t%s [options]" % sys.argv[0])
    print("options:")
    print("\t-d, --dbdf : DBDF of the FPGA, mandatory")
    print("\t-y, --yes : If specified, automatic yes to prompts")
    print("\t-h, --help : If specified, show usage\n")
    print("\tExample: %s 0000:17:0.1" % sys.argv[0])
    print("\n\tThis cmdline tool is used to mainly do card level reset.")
    print("\tEg. U30 card which has 2 FPGAs in one card. When whichever FPGA is")
    print("\tspecified, both FPGAs on the card will be reset. Please note, user")
    print("\tneeds to manually kill all processes running on the specified FPGA.")
    print("\tIf there is only one FPGA node in the card, the cmd will fall back")
    print("\tto 'xbutil reset'\n")
    print("\tRoot privilege is required to run this cmd\n")

def main():
    if (os.geteuid() != 0):
        print("ERROR: root privilege is required.")
        exit(1)
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hd:y", ["help", "dbdf=", "yes"])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        exit(1)

    dbdf = None
    yes = False
    for o, a in opts:
        if o in ("-y", "--yes"):
            yes = True
        elif o in ("-h", "--help"):
            usage()
            exit(1)
        elif o in ("-d", "--dbdf"):
            dbdf = a
        else:
            assert False, "unrecognized option"

    if not dbdf:
        print("ERROR: -d is mandatory")
        usage()
        exit(1)

    get_node_bus_mapping()
    mgmt = dbdf[:len(dbdf)-1] + "0"
    user = dbdf[:len(dbdf)-1] + "1"

    try:
        if mgmt not in node_bus_mapping:
            print("ERROR: Is %s a FPGA node?" % user)
            exit(1)
        with open(os.path.join(rootDir, user, "ready")) as f : ready = f.read()
        ready = ready.strip("\n")
        if ready == "0x0" or ready == "0":
            print("ERROR: FPGA %s is not usable" % user)
            exit(1)

        #print node_parent_mapping
        nodes_in_cards = bus_children[node_bus_mapping[mgmt]]
        #print nodes_in_cards
        if len(nodes_in_cards) == 1:
            #print("xbutil reset %s" % sys.argv[1])
            print("All existing processes will be killed.")
            while not yes:
                line = input("Are you sure you wish to proceed? [y/n]:")
                if line == "n":
                    exit(1)
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
            while not yes:
                line = input("Are you sure you wish to proceed? [y/n]:")
                if line == "n":
                    exit(1)
                if line == "y":
                    break
            rm_user = rm[:len(rm)-1] + "1"
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
        usage()

main()
