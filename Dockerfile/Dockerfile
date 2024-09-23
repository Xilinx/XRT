# Set the base image dynamically  
ARG BASE_IMAGE  
FROM ${BASE_IMAGE}  
  
# Set environment variables  
ARG PACKAGE_FILE  
  
# Copy the package into the image  
COPY ${PACKAGE_FILE} /tmp/  
  
# Install the package based on the base image  
RUN if [[ "${BASE_IMAGE}" == "centos:8" || "${BASE_IMAGE}" == "amazonlinux:2" ]]; then \  
      # For CentOS and Amazon Linux (RPM)  
      yum install -y epel-release && \  
      yum install -y dnf-plugins-core wget && \  
      yum clean all && \  
      yum localinstall -y /tmp/${PACKAGE_FILE} && \  
      rm -f /tmp/${PACKAGE_FILE} && \  
      # Verify the installation  
      rpm -qa | grep xrt; \  
    else \  
      # For Ubuntu (DEB)  
      apt-get update && \  
      apt-get install -y wget && \  
      dpkg -i /tmp/${PACKAGE_FILE} || apt-get install -f -y && \  
      rm -f /tmp/${PACKAGE_FILE} && \  
      # Verify the installation  
      dpkg -l | grep xrt; \  
    fi  
