#------------------------------ Source's ---------------------------------------

#Derzeit nur 32-Bit Anwendung ueber mingw64-w64-mingw32- compilierbar
#--> Trace32 auch als 32-Bit Variante gestartet werden

#Target = Dir-Name
PWD       = $(shell pwd)
TARGET    = $(shell basename $(PWD))
TARGETEXT = .dll
#TARGETEXT = .so    #MacOs+Linux Anpassung

MODULES             = sim_NXT.c simul.c pit.c aic.c spi.c twi.c pio.c nxt.c

#------------------------------ Modes ------------------------------------------

PROJECT_TARGET = RELEASE
LST_FILES = NO

.SILENT:

#------------------------------ Pfade & Programme ------------------------------

INSTALL_DIR = .
EXEDIR = bin
OBJDIR = obj
DEPDIR = dep	

MINGW32 = i686-w64-mingw32-
#MINGW32 =          MacOs+Linux Anpassung

CC   = $(MINGW32)gcc
C+   = $(MINGW32)g++
LD   = $(MINGW32)ld
AR   = $(MINGW32)ar
MKDIR = mkdir -p
TOUCH = touch
SED   = sed
#SED = LC_CTYPE=C LC_ALL=C  LANG=c sed                    #MacOs Anpassung


#------------------------------ Dependencys ------------------------------------
ifeq ($(PROJECT_TARGET),DEBUG)
DEB_FLAG = -DDEBUG
OPT_FLAG = -Wall -fno-builtin -g -O 
else ifeq ($(PROJECT_TARGET),RELEASE)
DEB_FLAG  =
OPT_FLAG  = -O2
else
$(error "Unknown Projekt_Target")
endif

ifeq ($(LST_FILES),YES)
LST_FLAG +=-Wa,-adhlns=$(subst $(suffix $<),.lst,$<) 
LST_FLAG +=-H
endif

#-------------------------------------------------------------------------------

LIBS   +=  
SHLIBS += 
#SHLIBS += -ldl -lrt -lm   nicht für MingGW

#------------------------------ Include Path -----------------------------------

INCLUDES       = 

#-------------------------------- C Flags --------------------------------------
ifeq ($(TARGETEXT),.so)
CC_OPTS += -fpic
endif

LDFLAGSDLL += -shared -Wl,--dll  $(TARGET).def
#LDFLAGS += -shared -Wl,--dll,--out-implib,$(LIBRARY) 
#LDFLAGS += -shared -Wl,--dll                          parse.def
LDFLAGSSO  +=  -shared

#64-Bit Funktion: wird derzeit nicht unterstützt
#CC_OPTS = -m64
CFLAGS     = $(CC_OPTS) $(DEB_FLAG) $(LST_FLAG) $(INCLUDES) $(OPT_FLAG)
CPPFLAGS   = $(CFLAGS) $(CPP_OPTS)

#----------------------------- Source Files ------------------------------------

SRCS      = $(MODULES) 
OBJS      =	$(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(SRCS))))

PRG_NAME  = $(EXEDIR)/$(TARGET)

PROGRAM   = $(PRG_NAME)$(TARGETEXT)

LIBRARY   = $(EXEDIR)/lib$(TARGET).a

#--------------------------------- Rules ---------------------------------------

all: $(OBJDIR)/.f $(DEPDIR)/.f $(EXEDIR)/.f  $(PROGRAM) install

clean: 
	rm -rf $(OBJDIR) $(DEPDIR) $(EXEDIR)
	find . -regex '.*~' -delete
	find . -regex '.*BK' -delete
	find . -regex '.*pre' -delete
	find . -name BARE$$ -type d -exec rm -r '{}' \;

install: 
	@echo "---> Copy $(PROGRAM) to $(INSTALL_DIR)"
	cp $(PROGRAM) $(INSTALL_DIR)

$(PRG_NAME).dll : $(OBJS) $(LIBS)
	@echo "-> Linking $(notdir $?) --> $(notdir $@)"
	$(CC) $(LDFLAGSDLL) -o $@ $(OBJS) $(LIBS) $(SHLIBS)
	@echo "---> Created DLL: '$@'"

$(PRG_NAME).so : $(OBJS) $(LIBS)
	@echo "-> Linking $(notdir $?) --> $(notdir $@)"
	$(CC) $(LDFLAGSSO) -o $@ $(OBJS) $(LIBS) $(SHLIBS)
	@echo "---> Created SO: '$@'"

-include $(addprefix ./$(DEPDIR)/,$(addsuffix .Po, $(basename $(SRCS))))

$(OBJDIR)/%.o : %.c
	@echo "-> Compiling $< to $(notdir $@)"
	$(CC) $(CFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	mv -f $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Po

$(OBJDIR)/%.o : %.cpp
	@echo "-> Compiling $< to $(notdir $@)"
	$(CC) $(CPPFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	mv -f $(DEPDIR)/$(notdir $*).Tpo $(DEPDIR)/$(notdir $*).Po
	
.PRECIOUS: %/.f
%/.f:
	@echo "-> MkDir  $(dir $@)"
	$(MKDIR) $(dir $@)
	$(TOUCH) $@
	