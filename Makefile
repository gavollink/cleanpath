CC=gcc
CCFLAGS=-Wall -O2
CCFLAGS+=-std=gnu99
ifdef DEBUG
	# Maintainer stuff only, you don't want to see this.
CCFLAGS+=-ggdb -DDEBUG
endif
#CCFLAGS+=-ggdb

INSTALLDIR=

FINAL=cleanpath
SOURCE=bstr.c cleanpath.c
X_DEPS=Makefile

SYS=$(shell uname -s)
ARCH=$(shell uname -m)
MAJ_VER=$(shell uname -r | awk -F. '{print $$1}')
# OS X 11, Big Sur, is the earliest macOS that supported arm64
TARGET_ARM64=-target arm64-apple-macos11
# OS X 10.10 is Yosemite, released 2014, which supports
# all hardware from 2010 or more recent.
TARGET_X86_64=-target x86_64-apple-macos10.10

ifeq ($(shell test "Darwin" = $(SYS) -a $(MAJ_VER) -ge 20; echo $$?), 0)
# This is a mac running Big Sur (11, D20) or NEWER:
# Darwin 20 is Big Sur, Big Sur is the earliest macOS that supported arm64.
# Any build platform here (x86_64 or arm64) will be able to target
# a universal binary with arm64-Big Sur and x86_64-Yosemite.
INTERIM=universal.$(FINAL)
else ifeq ($(shell test "Darwin" = $(SYS) -a $(MAJ_VER) -lt 14; echo $$?), 0)
# This is a mac running Mavericks (10.9, D13) or OLDER.
# Treating this the same as Linux, where we have no -target specified
# and just expect the compiler will do the right thing.
INTERIM=default.$(FINAL)
else ifeq ($(shell test "Darwin" = $(SYS); echo $$?), 0)
# This is a macOS between Yosemite (10.10, D14) and Catalina (10.15, D19)
# Yosemite is the earliest target macOS I own (or have access to),
INTERIM=$(ARCH).$(FINAL)
else
INTERIM=default.$(FINAL)
endif

$(FINAL): $(INTERIM)
	cp $(INTERIM) $(FINAL)

universal.$(FINAL): x86_64.$(FINAL) arm64.$(FINAL)
	lipo -create -output universal.$(FINAL) x86_64.$(FINAL) arm64.$(FINAL)

# x86_64 target is Yosemite (10.10, D14)
# Yosemite is the earliest target macOS I own (or have access to),
# You might have luck targeting something that's older, but I
# won't release what I cannot test.
# So that's where I'm pegging x86_64 specific targets.
# There's a comment at the end of the file for further info...
x86_64.$(FINAL): $(SOURCE) $(X_DEPS)
	$(CC) $(CCFLAGS) -o x86_64.$(FINAL) $(SOURCE) $(TARGET_X86_64)

# OS X 11, Big Sur is the first arm64 capable version, so it is the lowest
# target possible for arm64.
arm64.$(FINAL): $(SOURCE) $(X_DEPS)
	$(CC) $(CCFLAGS) -o arm64.$(FINAL) $(SOURCE) $(TARGET_ARM64)

default.$(FINAL): $(SOURCE) $(X_DEPS)
	$(CC) $(CCFLAGS) -o default.$(FINAL) $(SOURCE)

install: $(FINAL)
	@if [ -d "$(INSTALLDIR)" ]; then \
		install -m 775 -C $(FINAL) $(HOME)/sbin; \
	elif [ -z "$(INSTALLDIR)" ]; then \
		echo "Cannot install, 'INSTALLDIR=', is not set." ; \
		echo "TRY: $$ make INSTALLDIR=<path> install" ; \
		false ;\
	else \
		echo "Cannot install, 'INSTALLDIR=$(INSTALLDIR)', does not exist." ; \
		false ;\
	fi

clean:
	-rm -f $(FINAL).o $(FINAL)*.obj *.$(FINAL)

dist-clean: clean
	-rm -f $(FINAL)

# FROM WIKIPEDIA...
#
#   NAME       (Moc OS X Version , Darwin Kernel Version)
# Cheetah 		( 10,    1.3.1    )  POWERPC ONLY (32 bit only)
# Puma 			( 10.1,  1.4.1 or 5 )
# Jaguar 	 	( 10.2,  6        )  First to support 64 bit POWERPC)
# Panther 		( 10.3,  7        )
# Tiger 		( 10.4,  8        )  First to support INTEL (32bit Darwin)
# Leopard 		( 10.5,  9        )
# Snow Leopard 	( 10.6,  10       )  Last support POWERPC, first 64bit Darwin
# Lion 			( 10.7,  11       )
# Mountain Lion	( 10.8,  12       )  64bit INTEL Darwin only
# Mavericks 	( 10.9,  13       )
# Yosemite 		( 10.10, 14       )  My oldest MBAir is here
# El Capitian 	( 10.11, 15       )
# Sierra 		( 10.12, 16       )
# High Sierra 	( 10.13, 17       )
# Mojave 		( 10.14, 18       )
# Catalina 		( 10.15, 19       )  Drops ALL 32bit app support
# Big Sur 		( 11, 	 20       )  Introduction of arm64
# Monterey		( 12, 	 21       )
# Ventura 		( 13, 	 22       )
# Sonoma  		( 14, 	 23       )  I don't have access to this yet
#
# So Tiger (10.4, D8) on 64bit would be the oldest theoretical x86_64 target
# but Mountain Lion (10.8, D12) is more realistic because it's the first
# place we're guaranteed to see a 64bit Darwin.
# I don't even know if it CAN be targeted anymore, or if it would work.
# So, since I can test Yosemite and know it works, that is what this
# Makefile does, the info here is for those who have older stuff and want to
# fiddle around with trying to support it with a UNIVERSAL binary.
#
# Real talk... All Apple hardware sold from 2010 on supports Yosemite
# or newer.  That's 13 years ago, which is forever for Apple hardware.
#
# EOF Makefile
