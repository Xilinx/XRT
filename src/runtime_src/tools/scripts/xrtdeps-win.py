# =============================================================================
# Checks and validates the windows installation environment
# =============================================================================
#
# How to use this script.
# 1) Create an "x64 Native Tools compmand Prompt" via the Start menues
# 2) Invoke this script: xrtdeps-win.py
#
#
# Details
# -------
# The following directory structure will be produced if all of the options
# are used:
#    C:\Xilinx
#       +--XRT
#          +--ext
#             +--bin
#             +--build
#             |  +--boost
#             |  +--OpenCL-Headers
#             |  +--OpenCL-ICD-Loader
#             +--include
#             |  +--boost-1_70
#             |  +--CL
#             +--lib
# =============================================================================

# -- Imports ------------------------------------------------------------------
import argparse
from argparse import RawDescriptionHelpFormatter
import os
import platform
import subprocess
import pathlib
import distutils
from distutils import dir_util
import glob
import shutil
from shutil import copyfile
import urllib.request 


# -- Global Variables --------------------------------------------------------
COMPILER = "Visual Studio 15 2017 Win64"

if platform.system() == 'Windows':
  XRT_LIBRARY_INSTALL_DIR = "c:/Xilinx/XRT/ext"
  XRT_LIBRARY_BUILD_DIR = "c:/Xilinx/XRT/ext/build/"
else:
  XRT_LIBRARY_INSTALL_DIR = "/mnt/c/Xilinx/XRT/ext"
  XRT_LIBRARY_BUILD_DIR = "/mnt/c/Xilinx/XRT/ext/build"

# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined.
#
def main():
  # -- Configure the argument parser
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  validates the XRT Windows build environment\n  gets and installs XRT supporting libraries')
  parser.add_argument('--boost', nargs='?', const='complete', default='skip', choices=['complete', 'minimal', 'skip'], help='install boost libraries')
  parser.add_argument('--icd', action="store_true", help='install Khronos OpenCL icd loader library')
  parser.add_argument('--opencl', action="store_true", help='install Khronos OpenCL header files')
  parser.add_argument('--gtest', action="store_true", help='install gtest libraries')
  parser.add_argument('--validate_all_requirements', action="store_true", help='validate all XRT dependent libraries and tools are installed')
  parser.add_argument('--verbose', action="store_true", help='enables script verbosity')
  args = parser.parse_args()


  # -- Libraries to get, build, and install --
  libraries = []

  # -- Boost Library
  boostLibraryObj = BoostLibrary(args.boost)
  libraries.append( boostLibraryObj )  

  # -- OpenCL Headers
  openCLHeaderObj = OpenCLHeaders(args.opencl)
  libraries.append( openCLHeaderObj ) 

  # -- OpenCL ICD Library
  icdLibraryObj = ICDLibrary(args.icd)
  libraries.append( icdLibraryObj )       

# -- gtest Library
  gtestLibraryObj = GTestLibrary(args.gtest)
  libraries.append( gtestLibraryObj )

  # -- Evaluate the options --------------------------------------------------
  verbose = args.verbose

  # -- Validate --
  if args.validate_all_requirements == True:
    validateTools( True, verbose )
    validateLibraries( libraries, True, verbose )
    buildConflicts( libraries, True, True, verbose)
    return False

  # -- Check for existing tools ----------------------------------------------
  if validateTools( True, verbose ) == False:
    print ("")
    print ("ERROR: Missing supporting build tools.")
    return True

  # -- Check for existing build conflicts ------------------------------------
  if buildConflicts( libraries, False, True, verbose ) == True:
    print ("")
    print ("ERROR: Existing build present for the given library to be installed.")
    print ("       Please remove this directory prior re-installing the library.")
    return True

  # -- Check library dependencies --------------------------------------------
  if icdLibraryObj.skipBuild() == False and openCLHeaderObj.skipBuild() == True:
    if openCLHeaderObj.isInstalled(False, verbose) == False:
      print ("")
      print ("ERROR: The library '" + icdLibraryObj.getName() + "' is dependent on the library '" + openCLHeaderObj.getName() +"'")
      print ("       which isn't installed nor to be installed on the command line.")
      print ("")
      print ("       Please either install it or add it as an option to be installed.")
      return True

  # -- Install the given libraries -------------------------------------------
  if installLibraries( libraries, verbose) == True:
    return True

  return False

#==============================================================================
# -- Class: BoostLibrary ------------------------------------------------------
#==============================================================================
class BoostLibrary:
  # --
  def __init__(self, skip):
    self.install_dir = XRT_LIBRARY_INSTALL_DIR
    self.root_build_dir = XRT_LIBRARY_BUILD_DIR
    self.tag_version = "boost-1.70.0"
    self.library_version = "1_70"
    self.skipBuildInstall = skip
    self.buildDir = "boost"

  # --
  def getName(self):
    return self.tag_version

  # --
  def isInstalled(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    boostInclude = os.path.join(self.install_dir, "include", "boost-" + self.library_version)

    if echo == True:
      print ("  " + self.getName() + " .................................... ", end='')

    if not os.path.exists(boostInclude) or not os.path.isdir(boostInclude):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Expected directory: " + boostInclude)
      return False

    if echo == True:
      print ("[installed]")
      if verbose == True:
        print ("    Found directory: " + boostInclude)

    return True

  # --
  def isBuildPresent(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    if echo == True:
      print ("  " + self.getName() + " build directory .................... ", end='')

    boostBuild = os.path.join(self.root_build_dir, "boost")
    if not os.path.exists(boostBuild) or not os.path.isdir(boostBuild):
      if echo == True:
        print ("[not found]")
        if verbose == True:
          print ("    Expected directory: " + boostBuild)
      return False

    if echo == True:
      print ("[found]")
      if verbose == True:
        print ("    Found directory: " + boostBuild)

    return True


  # --
  def getBuildAndInstallLibraryMinimum(self, verbose):
    # --
    print ("\n============================================================== ")
    print ("Starting Minimum Boost build")
    print ("Currently, this flow only does a 'minimum' library build as")
    print ("opposed to a 'minimum' git clone.")
    print ("============================================================== ")

    if verbose == True:
      print ("Creating Build Directory: " + self.root_build_dir)

    pathlib.Path(self.root_build_dir).mkdir(parents=True, exist_ok=True) 
    os.chdir(self.root_build_dir)

    if verbose == True:
      print ("Retrieving git repository...")

    cloneCmd = "git clone --recursive --branch=" + self.tag_version + "  https://github.com/boostorg/boost.git"
    print (cloneCmd)
    os.system(cloneCmd)

    boostCloneDir = os.path.join(self.root_build_dir, self.buildDir)
    if os.path.exists(boostCloneDir) == False:
      print ("ERROR: Unable to successfully retrieve the boost repository")
      print ("       Boost clone directory missing: " + boostCloneDir)
      return True

    # -- Build and release the boost repository
    if verbose == True:
      print ("Invoking boost bootstrap.bat...")

    os.chdir(boostCloneDir)
    cmd = "bootstrap.bat"
    print (cmd)
    os.system(cmd)

    if verbose == True:
      print ("Building and installing the boost library...")

    cmd = "b2 -j6 install toolset=msvc-14.1 --prefix=" + self.install_dir + " --build-type=complete address-model=64 architecture=x86 link=static threading=multi --with-filesystem --with-program_options --with-system"
    print (cmd)
    os.system(cmd)

    return False

  # --
  def getBuildAndInstallLibrary(self, verbose):
    
    if self.skipBuildInstall == "minimal":
      return self.getBuildAndInstallLibraryMinimum(verbose)

    # --
    print ("\n============================================================== ")
    print ("Starting FULL Boost build")
    print ("============================================================== ")

    if verbose == True:
      print ("Creating Build Directory: " + self.root_build_dir)

    pathlib.Path(self.root_build_dir).mkdir(parents=True, exist_ok=True) 
    os.chdir(self.root_build_dir)

    if verbose == True:
      print ("Retrieving git repository...")

    cloneCmd = "git clone --recursive --branch=" + self.tag_version + "  https://github.com/boostorg/boost.git"
    print (cloneCmd)
    os.system(cloneCmd)

    boostCloneDir = os.path.join(self.root_build_dir, self.buildDir)
    if os.path.exists(boostCloneDir) == False:
      print ("ERROR: Unable to successfully retrieve the boost repository")
      print ("       Boost clone directory missing: " + boostCloneDir)
      return True

    # -- Build and release the boost repository
    if verbose == True:
      print ("Invoking boost bootstrap.bat...")

    os.chdir(boostCloneDir)
    cmd = "bootstrap.bat"
    print (cmd)
    os.system(cmd)

    if verbose == True:
      print ("Building and installing the boost library...")

    cmd = "b2 -j6 install toolset=msvc-14.1 --prefix=" + self.install_dir + " --build-type=complete"
    print (cmd)
    os.system(cmd)

    return False

  # --
  def skipBuild(self):
    return self.skipBuildInstall == 'skip'


      


#==============================================================================
# -- Class: OpenCLLibrary -----------------------------------------------------
#==============================================================================
class OpenCLHeaders:
  # --
  def __init__(self, skip):
    self.install_dir = XRT_LIBRARY_INSTALL_DIR
    self.root_build_dir = XRT_LIBRARY_BUILD_DIR
    self.skipBuildInstall = skip
    self.headerDir = "CL"
    self.buildDir = "OpenCL-Headers"

  # --
  def getName(self):
    return "Khronos OpenCL Headers"

  # --
  def isInstalled(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    openCLInclude = os.path.join(self.install_dir, "include", self.headerDir)

    if echo == True:
      print ("  Khronos OpenCL headers .......................... ", end='')

    headerFilePath1 = os.path.join(openCLInclude, "cl.h")
    if not os.path.exists(headerFilePath1) or not os.path.isfile(headerFilePath1):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Missing header: " + headerFilePath1)
      return False

    headerFilePath2 = os.path.join(openCLInclude, "cl_ext.h")
    if not os.path.exists(headerFilePath2) or not os.path.isfile(headerFilePath2):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Missing header: " + headerFilePath2)
      return False

    headerFilePath3 = os.path.join(openCLInclude, "cl2.hpp")
    if not os.path.exists(headerFilePath1) or not os.path.isfile(headerFilePath1):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Missing header: " + headerFilePath3)
      return False

    if echo == True:
      print ("[installed]")
      if verbose == True:
        print ("    Found header: " + headerFilePath1)
        print ("    Found header: " + headerFilePath2)

    return True

  # --
  def isBuildPresent(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    if echo == True:
      print ("  Khronos OpenCL headers build directory .......... ", end='')

    buildDir = os.path.join(self.root_build_dir, self.buildDir)
    if not os.path.exists(buildDir) or not os.path.isdir(buildDir):
      if echo == True:
        print ("[not found]")
        if verbose == True:
          print ("    Expected directory: " + buildDir)
      return False

    if echo == True:
      print ("[found]")
      if verbose == True:
        print ("    Found directory: " + buildDir)

    return True

  # --
  def getBuildAndInstallLibrary(self, verbose):
    # -- 
    print ("\n============================================================== ")
    print ("Starting OpenCL-Headers build")
    print ("============================================================== ")

    if verbose == True:
      print ("Creating Build Directory: " + self.root_build_dir)

    pathlib.Path(self.root_build_dir).mkdir(parents=True, exist_ok=True) 
    os.chdir(self.root_build_dir)

    if verbose == True:
      print ("Retrieving git repository...")

    gitDir = os.path.join(self.root_build_dir, self.buildDir)
    cloneCmd = "git clone https://github.com/KhronosGroup/OpenCL-Headers.git " + gitDir
    print (cloneCmd)
    os.system(cloneCmd)

    if os.path.exists(gitDir) == False:
      print ("ERROR: Unable to successfully retrieve the Khronos OpenCl-Headers repository")
      print ("       Khronos OpenCL-Headers clone directory missing: " + gitDir)
      return True

    # -- Get the cl2.hpp file
    destcl2hpp = os.path.join(self.root_build_dir, self.buildDir, "CL", "cl2.hpp")
    urllib.request.urlretrieve("https://github.com/KhronosGroup/OpenCL-CLHPP/releases/download/v2.0.10/cl2.hpp", destcl2hpp)

    # -- Copy the header files
    srcDir = os.path.join(gitDir, "CL")
    destDir = os.path.join(self.install_dir, "include", "CL")

    if verbose == True:
      print ("Creating destination directory: " + destDir)

    pathlib.Path(destDir).mkdir(parents=True, exist_ok=True) 

    if verbose == True:
      print ("Copying header directory.")
      print ("   Source      : " + srcDir)
      print ("   Destination : " + destDir)

    distutils.dir_util.copy_tree(srcDir, destDir)

    return False

  # --
  def skipBuild(self):
    return self.skipBuildInstall == False



#==============================================================================
# -- Class: ICDLibrary --------------------------------------------------------
#==============================================================================
class ICDLibrary:
  # --
  def __init__(self, skip):
    self.install_dir = XRT_LIBRARY_INSTALL_DIR
    self.root_build_dir = XRT_LIBRARY_BUILD_DIR
    self.skipBuildInstall = skip
    self.headerDir = "CL"
    self.buildDir = "OpenCL-ICD-Loader"

  # --
  def getName(self):
    return "Khronos OpenCL ICD"

  # --
  def isInstalled(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    if echo == True:
      print ("  Khronos OpenCL ICD Libraries .................... ", end='')

    libraryFilePath1 = os.path.join(self.install_dir, "bin", "OpenCL.dll")
    if not os.path.exists(libraryFilePath1) or not os.path.isfile(libraryFilePath1):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Missing library: " + libraryFilePath1)
      return False

    libraryFilePath2 = os.path.join(self.install_dir, "lib", "OpenCL.lib")
    if not os.path.exists(libraryFilePath2) or not os.path.isfile(libraryFilePath2):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Missing library: " + libraryFilePath2)
      return False

    if echo == True:
      print ("[installed]")
      if verbose == True:
        print ("    Library found: " + libraryFilePath1)
        print ("    Library found: " + libraryFilePath2)

    return True

  # --
  def isBuildPresent(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    if echo == True:
      print ("  Khronos OpenCL ICD build directory .............. ", end='')

    buildDir = os.path.join(self.root_build_dir, self.buildDir)
    if not os.path.exists(buildDir) or not os.path.isdir(buildDir):
      if echo == True:
        print ("[not found]")
        if verbose == True:
          print ("    Expected directory: " + buildDir)
      return False

    if echo == True:
      print ("[found]")
      if verbose == True:
        print ("    Found directory: " + buildDir)

    return True


  # --
  def getBuildAndInstallLibrary(self, verbose):
    # -- 
    print ("\n============================================================== ")
    print ("Starting OpenCL-ICD-Loader build")
    print ("============================================================== ")

    if verbose == True:
      print ("Creating Build Directory: " + self.root_build_dir)

    pathlib.Path(self.root_build_dir).mkdir(parents=True, exist_ok=True) 
    os.chdir(self.root_build_dir)

    if verbose == True:
      print ("Retrieving git repository...")

    gitDir = os.path.join(self.root_build_dir, self.buildDir)
    cloneCmd = "git clone https://github.com/KhronosGroup/OpenCL-ICD-Loader.git " + gitDir
    print (cloneCmd)
    os.system(cloneCmd)

    if os.path.exists(gitDir) == False:
      print ("ERROR: Unable to successfully retrieve the Khronos OpenCl ICD Loader repository")
      print ("       Khronos OpenCL ICD Loader clone directory missing: " + gitDir)
      return True

    # -- Copy the header files
    srcDir = os.path.join(self.install_dir, "include", self.headerDir)
    dstDir = os.path.join(gitDir, "inc", self.headerDir)

    if verbose == True:
      print ("Creating destination directory: " + dstDir)

    pathlib.Path(dstDir).mkdir(parents=True, exist_ok=True) 

    if verbose == True:
      print ("Copying header directory.")
      print ("   Source      : " + srcDir)
      print ("   Destination : " + dstDir)

    distutils.dir_util.copy_tree(srcDir, dstDir)

    # -- Build the library
    buildingDir = pathlib.Path(gitDir, "build")

    if verbose == True:
      print ("Creating build directory: " + str(buildingDir))

    os.mkdir(buildingDir)
    os.chdir(buildingDir)

    # -- Create the build scripts
    if verbose == True:
      print ("Invoking cmake to create the build scripts...")

    cmd = "cmake -G \"Visual Studio 15 2017 Win64\" -DOPENCL_ICD_LOADER_REQUIRE_WDK=0 -DCMAKE_INSTALL_PREFIX=" + self.install_dir + " .."
    print (cmd)
    os.system(cmd)

    # -- Create the release library
    if verbose == True:
      print ("Invoking cmake build the release library...")

    cmd = "cmake --build . --verbose --config Release"
    print (cmd)
    os.system(cmd)

    # -- Install the library
    if verbose == True:
      print ("Invoking cmake build to install the library...")

    cmd = "cmake --build . --verbose --config Release --target install"
    print (cmd)
    os.system(cmd)

    return False;

  # --
  def skipBuild(self):
    return self.skipBuildInstall == False

#==============================================================================
# -- Class: GTestLibrary --------------------------------------------------------
#==============================================================================
class GTestLibrary:
  # --
  def __init__(self, skip):
    self.install_dir = XRT_LIBRARY_INSTALL_DIR
    self.root_build_dir = XRT_LIBRARY_BUILD_DIR
    self.skipBuildInstall = skip
    self.headerDir = "gtest"
    self.buildDir = "googletest"

  # --
  def getName(self):
    return "GTestLibrary"

  # --
  def isInstalled(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    gtestInclude = os.path.join(self.install_dir, "include", "gtest")

    if echo == True:
      print ("  " + self.getName() + " .................................... ", end='')

    if not os.path.exists(gtestInclude) or not os.path.isdir(gtestInclude):
      if echo == True:
        print ("[not installed]")
        if verbose == True:
          print ("    Expected directory: " + gtestInclude)
      return False

    if echo == True:
      print ("[installed]")
      if verbose == True:
        print ("    Found directory: " + gtestInclude)

    return True

  # --
  def isBuildPresent(self, echo, verbose):
    # Override echo if verbosity is enabled
    if verbose == True:
      echo = True;

    if echo == True:
      print ("  " + self.getName() + " build directory .................... ", end='')

    gtestBuild = os.path.join(self.root_build_dir, "googletest")
    if not os.path.exists(gtestBuild) or not os.path.isdir(gtestBuild):
      if echo == True:
        print ("[not found]")
        if verbose == True:
          print ("    Expected directory: " + gtestBuild)
      return False

    if echo == True:
      print ("[found]")
      if verbose == True:
        print ("    Found directory: " + gtestBuild)

    return True


  # --
  def getBuildAndInstallLibrary(self, verbose):
    # -- 
    print ("\n============================================================== ")
    print ("Starting GTest Library build")
    print ("============================================================== ")

    if verbose == True:
      print ("Creating Build Directory: " + self.root_build_dir)

    pathlib.Path(self.root_build_dir).mkdir(parents=True, exist_ok=True) 
    os.chdir(self.root_build_dir)

    if verbose == True:
      print ("Retrieving git repository...")

    gitDir = os.path.join(self.root_build_dir, self.buildDir)
    cloneCmd = "git clone https://github.com/google/googletest.git "+ gitDir
    print (cloneCmd)
    os.system(cloneCmd)

    if os.path.exists(gitDir) == False:
      print ("ERROR: Unable to successfully retrieve the Google-Test repository")
      print ("       Google-Test clone directory missing: " + gitDir)
      return True

    # -- Copy the header files
    srcDir = os.path.join(gitDir, "googletest", "include", "gtest")
    dstDir = os.path.join(XRT_LIBRARY_INSTALL_DIR, "include", "gtest")

    if verbose == True:
      print ("Creating destination directory: " + dstDir)

    pathlib.Path(dstDir).mkdir(parents=True, exist_ok=True) 

    if verbose == True:
      print ("Copying header directory.")
      print ("   Source      : " + srcDir)
      print ("   Destination : " + dstDir)

    distutils.dir_util.copy_tree(srcDir, dstDir)

    # -- Build the library
    buildingDir = pathlib.Path(gitDir, "build")

    if verbose == True:
      print ("Creating build directory: " + str(buildingDir))

    os.mkdir(buildingDir)
    os.chdir(buildingDir)

    # -- Create the build scripts
    if verbose == True:
      print ("Invoking cmake to create the build scripts...")

    cmd = "cmake -G \"Visual Studio 15 2017 Win64\" .."
    print (cmd)
    os.system(cmd)

	# -- Copying cmake files (GTestConfig, GTestConfigVersion)
    srcDir = os.path.join(buildingDir, "googletest", "generated")
    dstDir = os.path.join(XRT_LIBRARY_INSTALL_DIR, "lib", "cmake", "gtest")
    pathlib.Path(dstDir).mkdir(parents=True, exist_ok=True)
    distutils.dir_util.copy_tree(srcDir, dstDir)

    # -- Create the release library
    if verbose == True:
      print ("Invoking cmake build the release library...")

    cmd = "cmake --build . --verbose --config Release"
    print (cmd)
    os.system(cmd)

	# -- Create the Debug library
    if verbose == True:
      print ("Invoking cmake build the Debug library...")

    cmd = "cmake --build . --verbose --config Debug"
    print (cmd)
    os.system(cmd)

    # -- Copying Release .lib files
    if verbose == True:
      print ("Copying Release libraries...")

    srcDir = os.path.join(buildingDir, "lib", "Release")
    dstDir = os.path.join(XRT_LIBRARY_INSTALL_DIR, "lib")
    distutils.dir_util.copy_tree(srcDir, dstDir)

	# -- Copying Debug .lib files
    if verbose == True:
      print ("Copying Debug libraries...")

    srcDir = os.path.join(buildingDir, "lib", "Debug")
    dstDir = os.path.join(XRT_LIBRARY_INSTALL_DIR, "lib")
    distutils.dir_util.copy_tree(srcDir, dstDir)

    return False;

  # --
  def skipBuild(self):
    return self.skipBuildInstall == False

# ============================================================================
# Helper methods
# ============================================================================

# -- which --------------------------------------------------------------------
# Look for the executable in the path
def which( program ):
    # -- isExe ----------------------------------------------------------------
    def isExe(filePath):
        return os.path.isfile(filePath) and os.access(filePath, os.X_OK)

    filePath, fileName = os.path.split(program)
    if filePath:
        if isExe( program ):
            return program
    else:
        for aPath in os.environ["PATH"].split(os.pathsep):
            exeFile = os.path.join(aPath, program)
            if isExe(exeFile):
                return exeFile

    return None

# -- validateTools -------------------------------------------------------------
def validateTools( echo, verbose ):
  # -- supporting method
  def toolFoundNotFound(program, echo, verbose):
    exeFile = which(program)
    if exeFile is None:
      print ("[not found]")
      return False
    else:
      print ("[found]")
      if verbose == True:
        print ("    Path: " + exeFile)
      return True


  allToolsInstalled = True            # Assume all of the tools are installed
  if echo == True:
    print ("\n---------------------------------------------------------------------------")
    print ("Validating the installation XRT Windows supporting applications.")
    print ("---------------------------------------------------------------------------")

  # -- CL 
  print ("  CL .............................................. ", end='')
  if toolFoundNotFound("cl.exe", echo, verbose) == False:
    allToolsInstalled = False;

  # -- git
  print ("  git ............................................. ", end='')
  if toolFoundNotFound("git.exe", echo, verbose) == False:
    allToolsInstalled = False;

  # -- CMAKE
  print ("  CMAKE ........................................... ", end='')
  if toolFoundNotFound("cmake.exe", echo, verbose) == False:
    allToolsInstalled = False;

  return allToolsInstalled


# -- validateLibraries --------------------------------------------------------
def validateLibraries( libraries, echo, verbose ):
  if echo == True:
    print ("\n---------------------------------------------------------------------------")
    print ("Examining and validating the installation XRT Windows supporting libraries.")
    print ("---------------------------------------------------------------------------")

  for library in libraries:
    library.isInstalled( echo, verbose )


# -- buildConflicts -----------------------------------------------------------
def buildConflicts( libraries, checkAll, echo, verbose ):
  print ("\n---------------------------------------------------------------------------")
  print ("Examining system for existing library build directories.")
  print ("---------------------------------------------------------------------------")

  buildConflicts = False;
  # Examine only the libraries that are about to be installed
  for library in libraries:
    if checkAll == False and library.skipBuild() == True:
      continue

    if library.isBuildPresent(echo, verbose):
      buildConflicts = True

  return buildConflicts

# -- installLibraries ---------------------------------------------------------
def installLibraries( libraries, verbose ):
  for library in libraries:
    if library.skipBuild() == True:
      continue

    print ("\n---------------------------------------------------------")
    print ("Installing Library: " + library.getName())
    print ("---------------------------------------------------------")
    if library.getBuildAndInstallLibrary( verbose ) == True:
       print ("ERROR: Installing " + library.getName())
       return True

  return False


# -- Start executing the script functions
if __name__ == '__main__':
  if main() == True:
    print ("\nError(s) occurred.")
    exit(1)


