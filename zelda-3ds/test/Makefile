#---------------------------------------------------------------------------------
# Programme de test 3DS (citro3d) : vérifie la fabrique et l'émulateur.
# Basé sur le modèle des exemples devkitPro (3ds-examples).
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM n'est pas défini : utilisez l'image Docker devkitpro/devkitarm")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET		:=	zelda3ds-test
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
APP_TITLE	:=	Zelda 3DS - test
APP_DESCRIPTION	:=	Verification de la fabrique 3DS
APP_AUTHOR	:=	emulateur-ia

ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS	+=	$(INCLUDE) -D__3DS__
CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro3d -lctru -lm
LIBDIRS	:= $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SOURCES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export OFILES_BIN	:=	$(PICAFILES:.v.pica=.shbin.o)
export OFILES		:=	$(OFILES_BIN) $(OFILES_SOURCES)
export HFILES		:=	$(PICAFILES:.v.pica=_shbin.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(OUTPUT).smdh
export _3DSXFLAGS	+=	--smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf

else

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES) : $(HFILES)
$(OUTPUT).elf	:	$(OFILES)

.PRECIOUS	:	%.shbin
%.shbin.o %_shbin.h : %.shbin
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

endif
