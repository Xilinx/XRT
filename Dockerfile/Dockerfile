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
  
# Copy the extracted RPM and DEB files into the Docker image    
COPY ./path/to/extracted/files/* ${PACKAGE_PATH}/    
  
# Install XRT packages if RPM files are present    
RUN find ${PACKAGE_PATH} -name "*.rpm" -exec yum install -y {} +    
  
# Clean up    
RUN rm -rf ${PACKAGE_PATH}/*.rpm    
  
# Enable Systemctl (if needed)    
RUN wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py    
RUN echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc    
RUN ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl    
RUN export PATH=/usr/local/bin/:$PATH    
  
# Copy any additional scripts if needed    
COPY auto_setup.sh /opt/xilinx/auto_setup.sh    
