#!/bin/bash

attach_device()
{
	DOMAIN=$1
	PCI_ID=$2
	XML_FILE=$3
            #####################  Creating xml file of the pci device  #######################

	virsh nodedev-dumpxml --device $PCI_ID > temp.xml
	BUS0=$(xmllint --xpath 'string(//device/capability/bus)' temp.xml)
	SLOT0=$(xmllint --xpath 'string(//device/capability/slot)' temp.xml)
	FUNCTION0=$(xmllint --xpath 'string(//device/capability/function)' temp.xml)
	xmlstarlet ed -L -u '//hostdev/source/address/@bus' -v "$BUS0" $XML_FILE
	xmlstarlet ed -L -u '//hostdev/source/address/@slot' -v "$SLOT0" $XML_FILE
	xmlstarlet ed -L -u '//hostdev/source/address/@function' -v "$FUNCTION0" $XML_FILE
	rm -f temp.xml

	virsh attach-device $DOMAIN $XML_FILE --live
	sleep 5s
}
###########################################################################################################

attach_device ubuntu18.04 pci_0000_3b_00_1 .github/scripts/pipeline/template.xml
attach_device ubuntu18.04 pci_0000_3b_00_0 .github/scripts/pipeline/template.xml

attach_device centos7.8 pci_0000_5e_00_1 .github/scripts/pipeline/template.xml
attach_device centos7.8 pci_0000_5e_00_0 .github/scripts/pipeline/template.xml

attach_device ubuntu20.04 pci_0000_d8_00_1 .github/scripts/pipeline/template.xml
attach_device ubuntu20.04 pci_0000_d8_00_0 .github/scripts/pipeline/template.xml

