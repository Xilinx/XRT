FROM centos:8  
ARG PACKAGE_PATH=/tmp/xilinx_packages  
  
# Update and install necessary tools  
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-* && \  
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* && \  
    yum install -y epel-release && \  
    yum install -y wget tar && \  
    yum clean all  
  
# Create directory for XRT packages  
RUN mkdir -p ${PACKAGE_PATH}  
  
# Copy the tar file containing XRT packages into the Docker image  
COPY ../xrt_packages/xrt_202410.2.17.723_8.1.1911-x86_64-xrt.tar.gz ${PACKAGE_PATH}/  
  
# Extract the tar file  
RUN tar -zxvf ${PACKAGE_PATH}/xrt_202410.2.17.723_8.1.1911-x86_64-xrt.tar.gz -C ${PACKAGE_PATH}  
  
# Install XRT packages  
RUN yum install -y ${PACKAGE_PATH}/*.rpm  
  
# Clean up  
RUN rm -rf ${PACKAGE_PATH}/*.rpm ${PACKAGE_PATH}/xrt_202410.2.17.723_8.1.1911-x86_64-xrt.tar.gz  
  
# Enable Systemctl (if needed)  
RUN wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py  
RUN echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc  
RUN ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl  
RUN export PATH=/usr/local/bin/:$PATH  
  
# Copy any additional scripts if needed  
COPY auto_setup.sh /opt/xilinx/auto_setup.sh  
