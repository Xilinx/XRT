aie-pdi-transform
=================

The code in aie-pdi-transform directory provide functionality to transform a
given PDI to make it more efficient to load. For example multiple DMAs are
merged together and LOAD/STOREs are consolidated.

Please note that the coding style used in this subsystem is not conformant to
XRT coding style. Please do not use this code as an example to write new PRs
for XRT. In due course of time this subsystem will be rewritten in modern C++
like the rest of the XRT code base.
