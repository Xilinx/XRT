# No more Makefile: XRT.py (Working in progress)

> Python binding for XRT APIs

## Getting Started

### Prerequisite

#### Python and NumPy libraries

For Ubuntu, use `sudo apt install python-dev libpython-dev python-numpy`

For CentOS and RedHat, use `sudo yum install python-dev libpython-dev python-numpy`

#### Boost >= 1.63

For Ubuntu >= 18.04, `sudo apt install libboost-all-dev`

For other Linux distributions:

```bash
wget http://dl.bintray.com/boostorg/release/1.65.0/source/boost_1_65_0.tar.gz \
    && tar xfz boost_1_65_0.tar.gz \
    && cd boost_1_65_0 \
    && ./bootstrap.sh --prefix=/usr/local --with-libraries=python \
    && ./b2 install
```

Note: if installed in other location, use `export XRT_PYTHON_LIBRARY=/path/to/boost/installation`

#### XRT >= 2.1.0

For Ubuntu, use `sudo apt install xrt`

For CentOS and RedHat, use `sudo yum install xrt`

Refer to [XRT main repository](https://github.com/Xilinx/XRT)

### Installation

#### Install from source

1. Clone the repository

2. `cd ./XRT/contrib/python-xrt`

3. `python setup.py install`

#### Install using `pip` (working in progress)

For Python 2.x, use `pip install xrt`

For Python 3.x, use `pip3 install xrt`

#### Install using Linux Distribution (working in progress)

For Ubuntu, use `sudo apt install python-xrt`

For CentOS, RedHat, use `sudo yum install python-xrt`

#### Use Docker (working in progress)

Still planning ...