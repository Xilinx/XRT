ARG BASE_IMAGE    
FROM ${BASE_IMAGE}   
  
RUN echo "Using base image: ${BASE_IMAGE}"  # Debug statement   
  
ARG PACKAGE_FILE  
RUN echo "Using package file: ${PACKAGE_FILE}"  # Debug statement  

  
COPY ${PACKAGE_FILE} /tmp/     
COPY xcr_* /tmp/  
COPY xrm_* /tmp/ 
  
ARG INSTALL_COMMANDS  
RUN eval "${INSTALL_COMMANDS}"  
