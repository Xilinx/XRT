#!/bin/bash
sudo mkdir /tmp/host_files
sudo mount -t 9p -o trans=virtio,version=9p2000.L /hostshare /tmp/host_files
