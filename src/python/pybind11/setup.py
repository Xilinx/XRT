"""

 Copyright (C) 2016-2020 Xilinx, Inc

 Licensed under the Apache License, Version 2.0 (the "License"). You may
 not use this file except in compliance with the License. A copy of the
 License is located at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
  License for the specific language governing permissions and limitations
 under the License.

"""

import os
import sys
import platform
import subprocess

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        cmake_args = ['-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
                      '-DPYTHON_EXECUTABLE=' + sys.executable]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + 'Release']
        
        build_args = ['--config', 'Release']

        env = os.environ.copy()
        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(env.get('CXXFLAGS', ''),
                                                               self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
            
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env)
        subprocess.check_call(['cmake', '--build', '.'] + build_args, cwd=self.build_temp)


setup(
    name='pyxrt',
    version='1.0.0',
    author='Graham Schelle',
    author_email='graham.schelle@xilinx.com',
    description="Pybind11 package for Xilinx's XRT",
    ext_modules=[CMakeExtension('pyxrt')],
    cmdclass=dict(build_ext=CMakeBuild),
    zip_safe=False,
)
