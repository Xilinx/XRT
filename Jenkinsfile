pipeline {
    agent { label "xcolinlocbld12" }
    environment {
        WORKSPACE_LOCAL = "${WORKSPACE}"
    }
    stages {
    stage ("Get code from git"){
        steps { checkout([$class: 'GitSCM', branches: [[name: '*/master']], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'WipeWorkspace']], submoduleCfg: [], userRemoteConfigs: [[credentialsId: '1fea742c-5c93-4a47-9d24-1d8d33c9149a', url: 'https://github.com/Xilinx/XRT.git']]])
    }}

    stage ('Paralell Build'){
        failFast true
        parallel{
            stage ("Build XRT and create RPM"){
                agent { label "xcolinlocbld12" }
                environment {
                    DEBUG_DIR = "Debug_rpm"
                    REL_DIR = "Release_rpm"
                }
                steps { sh ''' docker run --rm -e DEBUG_DIR -e REL_DIR -u xbuild -v $WORKSPACE_LOCAL:/opt/xrt/ xrt-centos "cd build && sh build.sh && cd ${REL_DIR} && make package"
            '''
                }
                }
            stage ("Build XRT and create DEB"){
                agent { label "xcolinlocbld12" }
                environment {
                    DEBUG_DIR = "Debug_deb"
                    REL_DIR = "Release_deb"
                }
                steps { sh ''' docker run --rm -e DEBUG_DIR -e REL_DIR -u xbuild -v $WORKSPACE_LOCAL:/opt/xrt/ xrt-ubuntu /bin/bash -c "cd build && sh build.sh && cd ${REL_DIR} && make package"
            '''
                }
                }
            }
    }
    }
}
