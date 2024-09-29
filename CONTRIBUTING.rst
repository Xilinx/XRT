===================
Contributing to XRT
===================


Welcome to XRT! You can contribute to XRT in a variety of ways. You can report bugs and feature requests using `GitHub Issues <https://github.com/Xilinx/XRT/issues>`_. You can send patches which add new features to XRT or fix bugs in XRT. You can also send patches to update XRT documentation.


Reporting Issues
****************

When reporting issues on GitHub please include the following:

1. XRT version including git hash
2. Alveo platform name and version
3. Output of ``xbutil query``
4. Output of ``xbmgmt scan``
5. ``dmesg``
6. gdb stack trace of host application
7. Contents of xrt.ini (if used)


Contributing Code
*****************

Please use GitHub Pull Requests (PR) for sending code contributions. When sending code sign your work as described below. Be sure to use the same license for your contributions as the current license of the XRT component you are contributing to.


Sign Your Work
==============

Please use the *Signed-off-by* line at the end of your patch which indicates that you accept the Developer Certificate of Origin (DCO) defined by https://developercertificate.org/ reproduced below::

  Developer Certificate of Origin
  Version 1.1

  Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
  1 Letterman Drive
  Suite D4700
  San Francisco, CA, 94129

  Everyone is permitted to copy and distribute verbatim copies of this
  license document, but changing it is not allowed.


  Developer's Certificate of Origin 1.1

  By making a contribution to this project, I certify that:

  (a) The contribution was created in whole or in part by me and I
      have the right to submit it under the open-source license
      indicated in the file; or

  (b) The contribution is based upon previous work that, to the best
      of my knowledge, is covered under an appropriate open-source
      license and I have the right under that license to submit that
      work with modifications, whether created in whole or in part
      by me, under the same open-source license (unless I am
      permitted to submit under a different license), as indicated
      in the file; or

  (c) The contribution was provided directly to me by some other
      person who certified (a), (b) or (c) and I have not modified
      it.

  (d) I understand and agree that this project and the contribution
      are public and that a record of the contribution (including all
      personal information I submit with it, including my sign-off) is
      maintained indefinitely and may be redistributed consistent with
      this project or the open source license(s) involved.


Here is an example Signed-off-by line which indicates that the contributor accepts DCO::


  This is my commit message

  Signed-off-by: Jane Doe <jane.doe@example.com>


Code License
============

All XRT code is licensed under the terms `LICENSE <https://github.com/Xilinx/XRT/blob/master/LICENSE>`_ Your contribution will be accepted under the same license.

Please consult the table below for the brief summary of XRT license for various components.

====================  =========================
Component             License
====================  =========================
Linux xocl driver     GPLv2
Linux xclmgmt driver  GPLv2
Linux zocl driver     Dual GPLv2 and Apache 2.0
User space            Apache 2.0
====================  =========================
