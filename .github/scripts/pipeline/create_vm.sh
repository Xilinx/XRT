#!/bin/bash

# The VMs, which are already configured to github as runners, will be reverted to a state where XRT is not yet installed and no tests are run
# There will no dependencies from the previous run

virsh snapshot-revert --domain ubuntu20.04 --snapshotname ubuntu20.04
virsh snapshot-revert --domain ubuntu18.04 --snapshotname ubuntu18.04
virsh snapshot-revert --domain centos7.8 --snapshotname centos7.8
    sleep 30s

# virsh start is used because the snapshots were taken with VMs in shut off state
# When snapshots are taken in shut off state, both memory and state of VM is preserved
# Otherwise, only state of machine is preserved

virsh start ubuntu18.04
virsh start centos7.8
virsh start ubuntu20.04
# After restarting the VM, it needs some time for connecting to github actions
# To address this delay, a sleep of 30s is included
    sleep 30s

