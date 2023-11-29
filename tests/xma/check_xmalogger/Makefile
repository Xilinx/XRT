CC    = g++

xrt_install_prefix := "/opt/xilinx"
ifdef XRT_INSTALL_PREFIX
	xrt_install_prefix := ${XRT_INSTALL_PREFIX}
endif

CFLAGS       = -std=c++11 -fPIC -g -I. -I../plugins -I${xrt_install_prefix}/xrt/include -I${XMA_INCLUDE}
LDFLAGS      = -L${xrt_install_prefix}/xrt/lib -L${XMA_LIBS} -lxmaapi -lxrt_core

SOURCES = $(shell echo *.c)
HEADERS = $(shell echo *.h)
OBJECTS = $(SOURCES:.c=.o)
TARGET  = $(SOURCES:.c=.exe)
OUTPUT  = $(SOURCES:.c=.out)


%.o: %.c
	$(CC) -c $^ $(CFLAGS)

%.exe: %.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) > ./$(OUTPUT) 2>&1

.PHONY: all
all: $(TARGET) run



.PHONY : clean
clean:
	rm -rf $(OBJECTS) $(TARGET)

