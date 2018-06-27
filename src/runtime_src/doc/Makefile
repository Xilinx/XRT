KERNELDOC_URL := https://raw.githubusercontent.com/torvalds/linux/master/scripts/kernel-doc
KERNELDOC := ./kernel-doc

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

.PHONY : all clean

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

$(KERNELDOC) :
	curl -o $(KERNELDOC) $(KERNELDOC_URL) && chmod +x $(KERNELDOC)

$(OUTDIR)/index.html: core $(KERNELDOC) $(RST)
	mkdir -p $(OUTDIR)
	$(SPHINX) $(SRCDIR) $(OUTDIR)

clean:
	rm -f $(RST) $(KERNELDOC)
	rm -rf $(OUTDIR) core/
