MPTOP = ../$(MPTOP_IN)
CROSS_COMPILE = arm-none-eabi-

include $(MPTOP)/py/mkenv.mk

CROSS = 0

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h
MICROPY_PY_USSL = 1

FROZEN_MANIFEST = 

# include py core make definitions
include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -I$(INCEXTRA)
INC += -I$(INCEXTRA_PORTLIBS)

LD = $(CC)
CFLAGS += -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations
CFLAGS += $(INC) -Wall -Werror -Wdouble-promotion -std=c11 $(COPT)
CFLAGS += -DMICROPY_SSL_MBEDTLS=1 -DMBEDTLS_CONFIG_FILE='<mbedtls/config.h>' -D_GNU_SOURCE

LDFLAGS += -Wl,-Map=$@.map,--cref -Wl,--gc-sections

CSUPEROPT = -O2 # save some code space

# Tune for Debugging or Optimization
CFLAGS += -g  # always include debug info in the ELF
CFLAGS += -O2 -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections

# Flags for optional C++ source code
CXXFLAGS += $(filter-out -std=c99,$(CFLAGS))

LIBS =

SRC_LOCAL_C = \
	port_functions.c \
	mpthreadport.c \
	mphalport.c \
	shared/libc/printf.c

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_LOCAL_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_LOCAL_CXX:.cpp=.o))

all: $(BUILD)/libmicropython.a

$(BUILD)/libmicropython.a: $(OBJ)

include $(TOP)/py/mkrules.mk
