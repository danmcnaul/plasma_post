
ifeq ($(OS),Windows_NT)
	ifeq ($(PROCESSOR_ARCHITECTURE), AMD64)
		ARCH = win64
		CXXFLAGS = -m64
		TARGET	= plasma_post64
	else
		ARCH = win32
		CXXFLAGS = -m32
		TARGET	= plasma_post32
	endif
	EXE = .exe
else
	ARCH = linux
	CXXFLAGS = -m64
	TARGET	= plasma_post
	EXE = 
endif


# Versioning info
VERSION_H	:= version.h

MAJOR	:= 1
MINOR	:= 0
PATCH	:= 0

# Auto-generate build number from git or timestamp
BUILD	:= $(shell date +%Y%m%d%H%M%S)

CXX = g++
CYY = gcc

OBJ = plasma_post.o badCmdLine.o badIniFile.o Args.o
LIBOBJ = badCmdLine.o badIniFile.o Args.o

product:	$(TARGET)$(EXE)

$(TARGET)$(EXE):	plasma_post.o $(VERSION_H)
	$(CXX) $(CXXFLAGS) plasma_post.o -o $@ $(LIBOBJ) $(LOADLIBES) $(LDDFLAGS)
	@make clean

badCmdLine.o:	badCmdLine.cc
	$(CXX) $(CXXFLAGS) -c badCmdLine.cc -o $@

badIniFile.o:	badIniFile.cc
	$(CXX) $(CXXFLAGS) -c badIniFile.cc -o $@

Args.o:	Args.cc
	$(CXX) $(CXXFLAGS) -c Args.cc -o $@

plasma_post.o:	plasma_post.cc $(LIBOBJ) $(VERSION_H)
	$(CXX) $(CXXFLAGS) -c plasma_post.cc -o $@

$(VERSION_H):
	@echo "Generating Version Header ... "
	@echo "#pragma once" > $(VERSION_H)
	@echo "#define VERSION_MAJOR $(MAJOR)" >> $(VERSION_H)
	@echo "#define VERSION_MINOR $(MINOR)" >> $(VERSION_H)
	@echo "#define VERSION_PATCH $(PATCH)" >> $(VERSION_H)
	@echo "#define VERSION_BUILD \"$(BUILD)\"" >> $(VERSION_H)

.PHONY: clean
clean:
	@-rm $(OBJ) $(VERSION_H)
.PHONY: clobber
clobber:
	@make clean
	@-rm $(TARGET)
.PHONY: info
info:
	@echo "Building for: $(ARCH)"
.PHONY:	debug 
debug:	CXXFLAGS += -g -O0 -Wall
debug:	product
.PHONY:	release
release:	CXXFLAGS += -O3 -DNDEBUG
release:	product

