# This will not fail if it doesn't exist, but run ./configure first.
-include configure.mk

# If configure couldn't find ARG_MAX, this will build anyway.
ifdef NO_ARG_MAX
CCFLAGS+=-DNO_ARG_MAX=1
endif
ifdef DEBUG
	# Maintainer stuff only, you don't want to see this.
CCFLAGS+=-ggdb -DDEBUG
endif

exec_prefix:=$(prefix)
_INSTALLDIR=$(DESTDIR)/$(exec_prefix)/$(bindir)/
INSTALLDIR=$(shell echo "$(_INSTALLDIR)" | sed -e 's@//*@/@g')

FINAL=cleanpath
SOURCE=bstr.c cleanpath.c
X_DEPS=Makefile configure.mk configure.h bstr.h

# Linux or Darwin (probably), Darwin is the one I treat differently
SYS=$(shell uname -s)
# x86_64 or arm64 (usually)
ARCH=$(shell uname -m)
MAJ_VER=$(shell uname -r | awk -F. '{print $$1}')
# OS X 11, Big Sur, is the earliest macOS that supported arm64
TARGET_ARM64=-target arm64-apple-macos11
# OS X 10.10 is Yosemite, released 2014, which supports
# all hardware from 2010 or more recent.
TARGET_X86_64=-target x86_64-apple-macos10.10

BUILD_DIR=$(ARCH).OBJ
OBJS := $(foreach TT,$(SOURCE),$(patsubst %.c,$(BUILD_DIR)/%.o,$(TT) ) )

ifeq ($(SYS), Darwin)
ifdef SIGNID
X_DEPS+=keychain-unlock
else
X_DEPS+=no-keychain
endif
endif

ifeq ($(shell test "arm64" = $(ARCH); echo $$?), 0)
ALT_ARCH=x86_64
else
ALT_ARCH=arm64
endif

ifeq ($(shell test "Darwin" = $(SYS) -a $(MAJ_VER) -ge 20; echo $$?), 0)
# This is a mac running Big Sur (11, D20) or NEWER:
# Darwin 20 is Big Sur, Big Sur is the earliest macOS that supported arm64.
# Any build platform here (x86_64 or arm64) will be able to target
# a universal binary with arm64-Big Sur and x86_64-Yosemite.
INTERIM=universal.$(FINAL)
ALT_BUILD_DIR=$(ALT_ARCH).OBJ
ALT_OBJS := $(foreach TT,$(SOURCE),$(patsubst %.c,$(ALT_BUILD_DIR)/%.o,$(TT) ) )
# OS X 11, Big Sur, is the earliest macOS that supported arm64
TARGET_ARM64=-target arm64-apple-macos11
# OS X 10.10 is Yosemite, released 2014, which supports
# all hardware from 2010 or more recent.
TARGET_X86_64=-target x86_64-apple-macos10.10
else ifeq ($(shell test "Darwin" = $(SYS) -a $(MAJ_VER) -lt 14; echo $$?), 0)
# This is a mac running Mavericks (10.9, D13) or OLDER.
# Treating this the same as Linux, where we have no -target specified
# and just expect the compiler will do the right thing.
INTERIM=$(ARCH).$(FINAL)
else ifeq ($(shell test "Darwin" = $(SYS); echo $$?), 0)
# This is a macOS between Yosemite (10.10, D14) and Catalina (10.15, D19)
# Yosemite is the earliest target macOS I own (or have access to),
INTERIM=$(ARCH).$(FINAL)
# OS X 11, Big Sur, is the earliest macOS that supported arm64
TARGET_ARM64=-target arm64-apple-macos11
# OS X 10.10 is Yosemite, released 2014, which supports
# all hardware from 2010 or more recent.
TARGET_X86_64=-target x86_64-apple-macos10.10
else
INTERIM=$(ARCH).$(FINAL)
endif

$(FINAL): $(INTERIM)
ifeq ($(shell test "Darwin" = "$(SYS)"; echo $$?), 0)
	@if [ -x "/usr/bin/codesign" -a -x "/usr/bin/security" -a -n "$(SIGNID)" ]; \
		then \
			if [ -n "$(CHAIN)" ]; \
			then \
				echo "codesign -s \"$(SIGNID)\" --keychain \"$(CHAIN)\" $(INTERIM)"; \
				/usr/bin/codesign -s "$(SIGNID)" --keychain "$(CHAIN)" $(INTERIM); \
			else \
				echo "codesign -s \"$(SIGNID)\" $(INTERIM)"; \
				/usr/bin/codesign -s "$(SIGNID)" $(INTERIM); \
			fi; \
		fi
endif
	@echo "    # Copy intermediate target to final $(FINAL)"
	cp $(INTERIM) $(FINAL)

universal.$(FINAL): x86_64.$(FINAL) arm64.$(FINAL)
	@echo "    # Darwin specific: join builds into universal.$(FINAL)"
	lipo -create -output universal.$(FINAL) x86_64.$(FINAL) arm64.$(FINAL)

# Make sure to accomodate any ARCH, plus the Darwin targets...
$(ARCH).$(FINAL): $(OBJS)
	@echo "    # Linking $@ in `dirname $@` from $(OBJS)"
ifeq ($(shell test "Darwin" = "$(SYS)" -a "x86_64" = "$(ARCH)"; echo $$?), 0)
	$(CC) $(TARGET_X86_64) -o $@ $(OBJS)
else ifeq ($(shell test "Darwin" = "$(SYS)"; echo $$?), 0)
	$(CC) $(TARGET_ARM64) -o $@ $(OBJS)
else
	$(CC) -o $@ $(OBJS)
endif

$(ALT_ARCH).$(FINAL): $(ALT_OBJS)
	@echo "    # Linking $@ in `dirname $@` from $(ALT_OBJS)"
ifeq ($(shell test "Darwin" = "$(SYS)" -a "x86_64" = "$(ALT_ARCH)"; echo $$?), 0)
	$(CC) -DALTBUILD=1 $(TARGET_X86_64) -o $@ $(ALT_OBJS)
else ifeq ($(shell test "Darwin" = "$(SYS)"; echo $$?), 0)
	$(CC) -DALTBUILD=1 $(TARGET_ARM64) -o $@ $(ALT_OBJS)
else
	$(CC) -DALTBUILD=1 -o $@ $(ALT_OBJS)
endif

$(OBJS): $(BUILD_DIR)/%.o: %.c $(X_DEPS)
	@echo "    # Compiling $@ in `dirname $@` from $<"
	@echo "    # X_DEPS is $(X_DEPS)"
	@if [ ! -d "`dirname $@`" ]; \
		then echo "        # mkdir `dirname $@`";\
		mkdir `dirname $@`; fi
ifeq ($(shell test "Darwin" = "$(SYS)" -a "x86_64" = "$(ARCH)"; echo $$?), 0)
	$(CC) $(CCFLAGS) $(TARGET_X86_64) -c $< -o $@
else ifeq ($(shell test "Darwin" = "$(SYS)"; echo $$?), 0)
	$(CC) $(CCFLAGS) $(TARGET_ARM64) -c $< -o $@
else
	$(CC) $(CCFLAGS) -c $< -o $@
endif

$(ALT_OBJS): $(ALT_BUILD_DIR)/%.o: %.c $(X_DEPS)
	@echo "    # Compiling $@ in `dirname $@` from $< -- Cross Compile $(ALT_ARCH)"
	@echo "    # X_DEPS is $(X_DEPS)"
	@if [ ! -d "`dirname $@`" ]; \
		then echo "        # mkdir `dirname $@`";\
		mkdir `dirname $@`; fi
ifeq ($(shell test "Darwin" = "$(SYS)" -a "x86_64" = "$(ALT_ARCH)"; echo $$?), 0)
	$(CC) $(CCFLAGS) $(TARGET_X86_64) -c $< -o $@
else ifeq ($(shell test "Darwin" = "$(SYS)"; echo $$?), 0)
	$(CC) $(CCFLAGS) $(TARGET_ARM64) -c $< -o $@
else
	$(CC) $(CCFLAGS) -c $< -o $@
endif

configure.mk configure.h: configure
	./configure

no-keychain:
	-@if [ -e ./.keychain-unlock ]; then rm ./.keychain-unlock; fi
	@echo "####################################################################"
	@echo "#### macOS, SIGNID not set, will not attempt to codesign $(FINAL)"
	@echo "####################################################################"

keychain-unlock: macos-keychain-unlock
	@echo "running script macos-keychain-unlock (SIGNID is set)"
	@./macos-keychain-unlock

macos-keychain-unlock: Makefile
	@printf '#!/bin/bash\n' > macos-keychain-unlock
	@printf '\n_NEED_UNLOCK=1\n' >> macos-keychain-unlock
	@printf 'CK=`date -v-3m +"%%s"`\n' >> macos-keychain-unlock
	@printf 'if [ -e './.keychain-unlock' ]\n' >> macos-keychain-unlock
	@printf 'then\n' >> macos-keychain-unlock
	@printf '    TS=`stat -f"%%m" ./.keychain-unlock`\n' >> macos-keychain-unlock
	@printf '    if [ "$$TS" -gt "$$CK" ]\n' >> macos-keychain-unlock
	@printf '    then\n' >> macos-keychain-unlock
	@printf '       _NEED_UNLOCK=0\n' >> macos-keychain-unlock
	@printf '    fi\n' >> macos-keychain-unlock
	@printf 'fi\n' >> macos-keychain-unlock
	@printf 'if [ 1 = "$$_NEED_UNLOCK" ]\n' >> macos-keychain-unlock
	@printf 'then\n' >> macos-keychain-unlock
ifdef CHAIN
	@printf "        echo 'security unlock-keychain \"$(CHAIN)\" && touch ./.keychain-unlock'\n" >> macos-keychain-unlock
	@printf '        security unlock-keychain "$(CHAIN)" && touch ./.keychain-unlock\n' >> macos-keychain-unlock
else
	@printf "        echo 'security unlock-keychain && touch ./.keychain-unlock'\n" >> macos-keychain-unlock
	@printf '        security unlock-keychain && touch ./.keychain-unlock\n' >> macos-keychain-unlock
endif
	@printf 'fi\n' >> macos-keychain-unlock
	@echo "built $@"
	chmod a+x macos-keychain-unlock


install: $(FINAL)
	@if [ "/$(bindir)/" = "$(INSTALLDIR)" ]; then \
		echo "Cannot install, prefix= and DESTDIR=, both empty." ; \
		echo "TRY: $$ make INSTALLDIR=<path> install" ; \
		false; \
	fi
	@if [ -d "$(INSTALLDIR)" ]; then \
		echo "install -m 775 -C $(FINAL) $(INSTALLDIR)"; \
		install -m 775 -C $(FINAL) $(INSTALLDIR); \
	else \
		echo "Cannot install, 'INSTALLDIR=$(INSTALLDIR)', does not exist." ; \
		false ;\
	fi

clean:
	-rm -rf "$(BUILD_DIR)"
	@if [ -d "$(ALT_BUILD_DIR)" ]; then \
		echo 'rm -rf "$(ALT_BUILD_DIR)"'; \
		rm -rf "$(ALT_BUILD_DIR)"; \
	fi
	-rm -f *.$(FINAL)

dist-clean distclean: clean
	-rm -f "configure.h"
	-rm -f $(FINAL)
	@if [ -e "configure.mk" ]; then \
		echo "##############################################################"; \
		echo "#### configure.mk exists, remove manually..."; \
		echo "#### rm configure.mk"; \
		echo "##############################################################"; \
		false; \
	fi

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
# vim: ft=make
# EOF Makefile
