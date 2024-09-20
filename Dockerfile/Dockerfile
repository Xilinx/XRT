# Use CentOS 8 as the base image  
FROM centos:8  
  
# Set environment variables  
ARG PACKAGE_FILE  
  
# Copy the RPM package into the image  
COPY ${PACKAGE_FILE} /tmp/  
  
# Install necessary tools and the RPM package  
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
