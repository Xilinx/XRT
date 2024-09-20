# Use CentOS 8 as the base image    
FROM centos:8    
    
# Set environment variables    
ARG PACKAGE_FILE    
    
# Copy the RPM package into the image    
COPY ${PACKAGE_FILE} /tmp/    
    
# Add a specific mirror URL to avoid DNS issues    
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-* && \    
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://mirror.centos.org|g' /etc/yum.repos.d/CentOS-* && \  
    echo "baseurl=http://mirror.centos.org/centos/8/BaseOS/x86_64/os/" >> /etc/yum.repos.d/CentOS-Base.repo && \  
    echo "baseurl=http://mirror.centos.org/centos/8/AppStream/x86_64/os/" >> /etc/yum.repos.d/CentOS-AppStream.repo  
    
# Update and install necessary tools and the RPM package    
RUN yum install -y epel-release && \    
    yum install -y dnf-plugins-core wget && \    
    yum config-manager --set-enabled powertools && \    
    yum clean all && \    
    yum install -y /tmp/${PACKAGE_FILE} && \    
    rm -f /tmp/${PACKAGE_FILE}    
    
# Verify the installation    
RUN rpm -qa | grep xrt    
  
# Set the entrypoint (if needed)    
# ENTRYPOINT ["/bin/bash"]  
