LEVEL := ..

DIR := $(notdir $(CURDIR))
EXENAME := $(DIR).exe

MYCLCFLAGS := -s

# MYCLLFLAGS := -s \
#  --nk addone:4 \
#  --sp addone_0.m_axi_gmem:bank0 \
#  --sp addone_1.m_axi_gmem:bank1 \
#  --sp addone_2.m_axi_gmem:bank2 \
#  --sp addone_3.m_axi_gmem:bank3 \
#  --nk copy:4 \
#  --sp copy_30.m_axi_gmem:bank0 \
#  --sp copy_01.m_axi_gmem:bank1 \
#  --sp copy_12.m_axi_gmem:bank2 \
#  --sp copy_23.m_axi_gmem:bank3 \

MYCLLFLAGS := -s \
 --nk addone:4 \
 --sp addone_0.m_axi_gmem:bank0 \
 --sp addone_1.m_axi_gmem:bank0 \
 --sp addone_2.m_axi_gmem:bank0 \
 --sp addone_3.m_axi_gmem:bank0 \
 --nk copy:4 \
 --sp copy_30.m_axi_gmem:bank0 \
 --sp copy_01.m_axi_gmem:bank0 \
 --sp copy_12.m_axi_gmem:bank0 \
 --sp copy_23.m_axi_gmem:bank0

include $(LEVEL)/common.mk
