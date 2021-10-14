#!/bin/bash

#refreshing the VM

virsh snapshot-revert --domain ubuntu20.04 --snapshotname ubuntu20.04
virsh snapshot-revert --domain ubuntu18.04 --snapshotname ubuntu18.04
virsh snapshot-revert --domain centos7.8 --snapshotname centos7.8
	sleep 30s
virsh start ubuntu18.04
virsh start centos7.8
virsh start ubuntu20.04
	sleep 30s

