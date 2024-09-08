FROM centos:8  
ARG PACKAGE_PATH=/tmp/xilinx_packages  
RUN yum upgrade -y; yum install -y wget; mkdir -p ${PACKAGE_PATH}  
  
# Add DNS configuration  
RUN echo "nameserver 8.8.8.8" > /etc/resolv.conf  
  
# Install necessary dependencies  
RUN yum install -y dnf-plugins-core  
RUN yum config-manager --set-enabled powertools  
RUN yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm  
RUN yum config-manager --set-enabled appstream  
  
# Copy XRT Packages from local filesystem  
COPY xrt_*.rpm ${PACKAGE_PATH}/  
  
# Install XRT and modify the Path  
RUN XRT_VERSION="202110.2.11.727"; \  
    yum install -y ${PACKAGE_PATH}/xrt_${XRT_VERSION}_8.1.1911-x86_64-xrt.rpm; \  
    mkdir /opt/xilinx/xrt_versions; \  
    mv /opt/xilinx/xrt /opt/xilinx/xrt_versions/xrt_${XRT_VERSION}; \  
    sed -i "s/\/opt\/xilinx\/xrt/\/opt\/xilinx\/xrt_versions\/xrt_$XRT_VERSION/" /opt/xilinx/xrt_versions/xrt_${XRT_VERSION}/setup.sh; \  
    sed -i "s/\/opt\/xilinx\/xrt/\/opt\/xilinx\/xrt_versions\/xrt_$XRT_VERSION/" /opt/xilinx/xrt_versions/xrt_${XRT_VERSION}/setup.csh; \  
    ln -s /opt/xilinx/xrt_versions/xrt_${XRT_VERSION} /opt/xilinx/xrt  
  
# Enable Systemctl  
RUN wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py  
RUN echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc  
RUN ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl  
RUN export PATH=/usr/local/bin/:$PATH  
  
COPY auto_setup.sh /opt/xilinx/auto_setup.sh  
