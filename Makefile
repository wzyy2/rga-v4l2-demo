
INCLUDES=-I. -I/usr/include/libdrm

CXXFLAGS=$(INCLUDES)

LDFLAGS= -ldrm -L.

define all-cpp-files-under
$(shell find $(1) -name "*."$(2) -and -not -name ".*" )
endef

define all-subdir-cpp-files
$(call all-cpp-files-under,.,"cpp")
endef

define all-subdir-c-files
$(call all-cpp-files-under,.,"c")
endef

CPPSRCS	 = $(call all-subdir-cpp-files)

CSRCS	 = $(call all-subdir-c-files)

CPPOBJS	:= $(CPPSRCS:.cpp=.o)
COBJS	:= $(CSRCS:.c=.o)

TARGETS=rga-v4l2

all:$(TARGETS)

rga-v4l2:$(CPPOBJS) $(COBJS)
	$(CXX) $(CXXFLAGS) $(CPPOBJS) $(COBJS) -o $(TARGETS) $(LDFLAGS)

$(CPPOBJS) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(COBJS) : %.o : %.c
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

clean:
	rm -f ./*.o $(TARGETS)


