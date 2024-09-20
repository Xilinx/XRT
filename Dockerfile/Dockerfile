# Use CentOS 8 as the base image    
FROM centos:8    
    
# Set environment variables    
ARG PACKAGE_FILE    
    
# Copy the RPM package into the image    
COPY ${PACKAGE_FILE} /tmp/    
    
# Update repository URLs and install necessary tools    
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-* && \    
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* && \    
    yum install -y epel-release && \    
    yum install -y dnf-plugins-core wget && \    
    yum config-manager --set-enabled powertools && \    
    yum clean all && \    
    yum install -y /tmp/${PACKAGE_FILE} && \    
    rm -f /tmp/${PACKAGE_FILE}    
    
# Verify the installation    
RUN rpm -qa | grep xrt    
  
# Set the entrypoint (if needed)    
# ENTRYPOINT ["/bin/bash"]  
