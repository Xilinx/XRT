ARG BASE_IMAGE  
FROM ${BASE_IMAGE}  
  
ARG PACKAGE_FILE  
  
# Debug: Print base image and package file  
RUN echo "---- Using Base Image ----" && cat /etc/os-release && \  
    echo "---- Package File: ${PACKAGE_FILE} ----"  
  
# Update and install necessary tools and dependencies  
RUN echo "---- Installing Dependencies ----" && \  
    if [ -f /etc/redhat-release ]; then \  
        echo "Detected Red Hat-based system" && \  
        yum install -y epel-release && \  
        yum install -y dnf-plugins-core wget && \  
        yum config-manager --set-enabled powertools && \  
        yum clean all && \  
        yum install -y ocl-icd ocl-icd-devel libyaml-devel; \  
    elif [ -f /etc/system-release ] && grep -q "Amazon Linux" /etc/system-release; then \  
        echo "Detected Amazon Linux" && \  
        yum install -y wget ocl-icd ocl-icd-devel libyaml-devel; \  
    else \  
        echo "Detected Debian-based system" && \  
        apt-get update && \  
        apt-get install -y wget ocl-icd-libopencl1 ocl-icd-opencl-dev libyaml-dev \  
        lsb-release dkms udev python3 uuid-dev libboost-filesystem1.74.0 \  
        libboost-program-options1.74.0 libgcc-s1 libncurses6 libprotobuf23 \  
        libssl3 libtinfo6 && \  
        apt-get clean; \  
    fi  
  
# Copy and install the package  
COPY ${PACKAGE_FILE} /tmp/  
RUN echo "---- Installing Package: ${PACKAGE_FILE} ----" && \  
    if [ "${PACKAGE_FILE##*.}" = "rpm" ]; then \  
        echo "Installing RPM package" && \  
        yum install -y /tmp/${PACKAGE_FILE}; \  
    else \  
        echo "Installing DEB package" && \  
        dpkg -i /tmp/${PACKAGE_FILE} || apt-get install -f -y; \  
    fi && \  
    rm -f /tmp/${PACKAGE_FILE}  
  
# Enable Systemctl (if needed)  
RUN echo "---- Setting up Systemctl Replacement ----" && \  
    wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py && \  
    echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc && \  
    ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl  
  
# Assuming auto_setup.sh is in the same directory as the Dockerfile  
COPY auto_setup.sh /opt/xilinx/auto_setup.sh  
