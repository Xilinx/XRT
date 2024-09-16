ARG BASE_IMAGE  
FROM ${BASE_IMAGE}  
  
ARG PACKAGE_FILE  
  
# Update and install necessary tools  
RUN if [ -f /etc/redhat-release ]; then \  
    yum install -y epel-release && \  
    yum install -y dnf-plugins-core wget && \  
    yum config-manager --set-enabled powertools && \  
    yum clean all && \  
    yum install -y ocl-icd ocl-icd-devel libyaml-devel; \  
elif [ -f /etc/system-release ] && grep -q "Amazon Linux" /etc/system-release; then \  
    yum install -y wget ocl-icd ocl-icd-devel libyaml-devel; \  
else \  
    apt-get update && \  
    apt-get install -y wget ocl-icd-libopencl1 ocl-icd-opencl-dev libyaml-dev && \  
    apt-get clean; \  
fi  
  
# Copy and install the package  
COPY ${PACKAGE_FILE} /tmp/  
RUN if [[ "${PACKAGE_FILE}" == *.rpm ]]; then \  
    echo "Installing RPM package: ${PACKAGE_FILE}" && \  
    yum install -y /tmp/${PACKAGE_FILE} && \  
    rpm -ql $(rpm -q --whatprovides /tmp/${PACKAGE_FILE}); \  
else \  
    echo "Installing DEB package: ${PACKAGE_FILE}" && \  
    dpkg -i /tmp/${PACKAGE_FILE} || apt-get install -f -y && \  
    dpkg-query -L $(dpkg -f /tmp/${PACKAGE_FILE} Package); \  
fi && \  
rm -f /tmp/${PACKAGE_FILE}  
  
# Enable Systemctl (if needed)  
RUN wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py  
RUN echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc  
RUN ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl  
  
# Assuming auto_setup.sh is in the same directory as the Dockerfile  
COPY auto_setup.sh /opt/xilinx/auto_setup.sh  
