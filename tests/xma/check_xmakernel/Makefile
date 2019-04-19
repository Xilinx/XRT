CC    = g++
CFLAGS       = -std=c++11 -fPIC -g -I. -I../plugins -I/opt/xilinx/xrt/include -I${XMA_INCLUDE}
LDFLAGS      = -L/opt/xilinx/xrt/lib -L${XMA_LIBS} -lxmaapi -lxrt_core

SOURCES = $(shell echo *.c)
HEADERS = $(shell echo *.h)
OBJECTS = $(SOURCES:.c=.o)
TARGET  = $(SOURCES:.c=.exe)
OUTPUT  = $(SOURCES:.c=.out)

#PREFIX = $(DESTDIR)/usr/local
#BINDIR = $(PREFIX)/bin

#%.o: %.c $(HEADERS)
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

