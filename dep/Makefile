# Makefile for dependencies

include ../env.mk


.PHONY: all rtld ifunc capstone keystone distorm3 clean realclean
all: rtld ifunc capstone keystone distorm3
	-@touch built
clean:
	-$(call short-make,rtld,clean)
	-$(call short-make,ifunc,clean)
	-@rm -f built
realclean:
	-$(call short-make,rtld,clean)
	-$(call short-make,ifunc,clean)
	-$(call short-make,distorm3/make/linux,clean)
	-$(call short-make,capstone,clean)
	-@rm -rf keystone/build
	-@rm -f built

ifeq ($(USE_LOADER),1)
rtld:
	$(call short-make,rtld)
else
rtld:
endif
ifunc:
	$(call short-make,ifunc)

distorm3:
	$(call short-make,distorm3/make/linux) CC='gcc -Wno-missing-braces'
	@cd distorm3/make/linux && \
	SOFILE=$$(find . -maxdepth 1 -name "libdistorm3.so.*" -type f ! -type l | sort -V | tail -n1) && \
	[ -n "$$SOFILE" ] && ln -sf "$$SOFILE" libdistorm3.so && ln -sf "$$SOFILE" libdistorm3.so.3; \
	echo "Linked $$SOFILE to libdistorm3.so"


ifeq ($(CROSS_ANALYZE),1)
CROSS2 =
else
CROSS2 = $(CROSS)
endif

# directly use make to install to 'lib'
CAPSTONE_INSTALL := $(abspath capstone/install)
capstone:
	@cd capstone \
		&& rm -fr install \
		&& env CAPSTONE_ARCHS='x86 aarch64' $(MAKE) CROSS=$(CROSS2) \
		&& env CAPSTONE_ARCHS='x86 aarch64' PREFIX=$(CAPSTONE_INSTALL) $(MAKE) CROSS=$(CROSS2) install

ifeq ($(USE_KEYSTONE),1)
keystone:
	-@cd keystone \
		&& mkdir build \
		&& cd build \
		&& ../make-share.sh \
		&& ../make-lib.sh
else
keystone:
endif
