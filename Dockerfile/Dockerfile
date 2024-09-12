FROM centos:8  
  
# Update and install necessary tools  
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-* && \  
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* && \  
    yum install -y epel-release && \  
    yum install -y dnf-plugins-core && \  
    yum config-manager --set-enabled powertools && \  
    yum clean all  
  
# Install missing dependencies  
RUN yum install -y ocl-icd ocl-icd-devel libyaml-devel  
  
# Assuming xrt.rpm is in the same directory as the Dockerfile  
COPY Dockerfile/xrt.rpm /tmp/xrt.rpm  
  
# Install the XRT package  
RUN yum install -y /tmp/xrt.rpm && rm -f /tmp/xrt.rpm  
  
# Enable Systemctl (if needed)  
RUN wget -cO - https://raw.githubusercontent.com/gdraheim/docker-systemctl-replacement/master/files/docker/systemctl3.py > /usr/local/share/systemctl3.py  
RUN echo "alias systemctl='python3 /usr/local/share/systemctl3.py'" >> /root/.bashrc  
RUN ln -s /usr/local/share/docker-systemctl-replacement/files/docker/systemctl3.py /usr/local/bin/systemctl  
RUN export PATH=/usr/local/bin/:$PATH  
  
# Assuming auto_setup.sh is in the same directory as the Dockerfile  
COPY Dockerfile/auto_setup.sh /opt/xilinx/auto_setup.sh  
