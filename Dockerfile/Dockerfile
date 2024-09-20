# Use CentOS 8 as the base image  
FROM centos:8  
  
# Set environment variables  
ARG PACKAGE_FILE=xrt_202410.2.17.833_8.1.1911-x86_64-xrt.rpm  
  
# Copy the RPM package and auto_setup.sh into the image  
COPY ${PACKAGE_FILE} /tmp/  
COPY auto_setup.sh /opt/xilinx/  
  
# Add a specific mirror URL to avoid DNS issues  
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-* && \  
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://mirror.centos.org|g' /etc/yum.repos.d/CentOS-*  
  
# Update and install necessary tools and the RPM package  
RUN yum install -y epel-release && \  
    yum install -y dnf-plugins-core wget && \  
    yum config-manager --set-enabled powertools && \  
    yum clean all && \  
    yum install -y /tmp/${PACKAGE_FILE} && \  
    rm -f /tmp/${PACKAGE_FILE}  
  
# Run the auto_setup.sh script  
RUN chmod +x /opt/xilinx/auto_setup.sh && \  
    /opt/xilinx/auto_setup.sh  
  
# Verify the installation  
RUN rpm -qa | grep xrt  
  
# Set the entrypoint (if needed)  
# ENTRYPOINT ["/bin/bash"]  
