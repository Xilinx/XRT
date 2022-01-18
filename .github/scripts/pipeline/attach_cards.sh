#!/bin/bash

# Libvirt snapshot cannot be taken with pci cards attached to VM
# Pci cards should be attached after doing virsh snapshot-revert
# For attaching pci cards, three values, bus, slot and function of pci card are required
# These values are included in xml file of pci device which is printed using virsh nodedev-dumpxml command

attach_device()
{
    DOMAIN=$1
    PCI_ID=$2
    XML_FILE=$3

    # virsh nodedev-dumpxml will print the xml file of pci device attached to host
    # This xml file will be redirected to temp.xml
    virsh nodedev-dumpxml --device $PCI_ID > temp.xml

    # temp.xml will be parsed to obtain bus, function and slot values
    BUS0=$(xmllint --xpath 'string(//device/capability/bus)' temp.xml)
    SLOT0=$(xmllint --xpath 'string(//device/capability/slot)' temp.xml)
    FUNCTION0=$(xmllint --xpath 'string(//device/capability/function)' temp.xml)

    # Above obtained values will be entered into template.xml file
    xmlstarlet ed -L -u '//hostdev/source/address/@bus' -v "$BUS0" $XML_FILE
    xmlstarlet ed -L -u '//hostdev/source/address/@slot' -v "$SLOT0" $XML_FILE
    xmlstarlet ed -L -u '//hostdev/source/address/@function' -v "$FUNCTION0" $XML_FILE
    rm -f temp.xml

    # The modified template.xml file is then used for attaching card to VM
    virsh attach-device $DOMAIN $XML_FILE --live

    # The enclave host issues attach-device request to VM. But when to attach device is left to VM itself
    # i.e., attach device may not happen right away. A sleep of 5s is included for this reason
    sleep 5s
}

# domain name, pci id(understandable by virsh) and template.xml file are provided as arguments for attach_device function

attach_device ubuntu18.04 pci_0000_3b_00_1 .github/scripts/pipeline/template.xml
attach_device ubuntu18.04 pci_0000_3b_00_0 .github/scripts/pipeline/template.xml

attach_device centos7.8 pci_0000_5e_00_1 .github/scripts/pipeline/template.xml
attach_device centos7.8 pci_0000_5e_00_0 .github/scripts/pipeline/template.xml

attach_device ubuntu20.04 pci_0000_d8_00_1 .github/scripts/pipeline/template.xml
attach_device ubuntu20.04 pci_0000_d8_00_0 .github/scripts/pipeline/template.xml

