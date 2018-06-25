KERNELDOC := /lib/modules/$(shell uname -r)/build/scripts/kernel-doc

SPHINX := sphinx-build

XRTSRCROOT := ..

OUTDIR := html

SRCDIR := toc

RST :=   \
	core/mgmt-ioctl.rst \
	core/xclhal2.rst \
	core/xocl_ioctl.rst \
	core/ert.rst

$(OUTDIR)/index.html : $(wildcard $(SRCDIR)/*.rst) $(RST)

all:  $(OUTDIR)/index.html 

core/mgmt-ioctl.rst : $(XRTSRCROOT)/driver/xclng/include/mgmt-ioctl.h
	$(KERNELDOC) -rst $< > $@

core/xocl_ioctl.rst : $(XRTSRCROOT)/driver/xclng/include/xocl_ioctl.h
	$(KERNELDOC) -rst $< > $@

core/xclhal2.rst : $(XRTSRCROOT)/driver/include/xclhal2.h
	$(KERNELDOC) -rst $< > $@

core/ert.rst : $(XRTSRCROOT)/driver/include/ert.h
	$(KERNELDOC) -rst $< > $@

core : 
	mkdir -p core/

$(OUTDIR)/index.html: core $(RST)
	mkdir -p $(OUTDIR)
	$(SPHINX) $(SRCDIR) $(OUTDIR)

clean:
	rm -rf $(OUTDIR)
	rm -f $(RST)
