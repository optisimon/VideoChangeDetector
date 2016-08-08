DESTDIR?=""

INCLUDE= `pkg-config --cflags gstreamer-1.0 gstreamer-plugins-base-1.0`
LIBS= `pkg-config --libs gstreamer-1.0 gstreamer-plugins-base-1.0` -lgstapp-1.0 -lgstvideo-1.0 -lpthread

VideoChangeDetector_OBJS= main.o
VideoChangeDetector_LIBS= $(LIBS)

EXECS= VideoChangeDetector
EXEC_installed= VideoChangeDetector

COMPILER_FLAGS+= -Wall -O3 -std=c++0x -ggdb

VideoChangeDetector: $(VideoChangeDetector_OBJS) $(wildcard *.h) $(wildcard *.hpp) Makefile
	$(CXX) $(COMPILER_FLAGS) -o $@ $($@_OBJS) $($@_LIBS)

%.o:	%.cpp
	$(CXX) -c $(COMPILER_FLAGS) -o $@ $< $(INCLUDE)

all: VideoChangeDetector

.PHONY: install
install: $(EXEC_installed)
	install $(EXEC_installed) $(DESTDIR)/usr/local/bin

.PHONY: uninstall
uninstall:
	rm $(DESTDIR)/usr/local/bin/$(EXEC_installed)

.PHONY: prepare
prepare:
	apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

.PHONY: clean
clean:
	rm -f $(EXECS) $(VideoChangeDetector_OBJS)
