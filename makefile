#
# hl2_relay makefile
#

OS := $(shell uname)

# $(info    OS is $(OS))

ifeq ($(OS), Darwin)
	CC  = clang
	LL  = -lm -lpthread
else
ifeq ($(OS), Linux)
	CC  = cc
	LL = -lm -lpthread
	STD = -std=c99
else
	$(error OS not detected)
endif
endif

FILES =  hl2_relay.c 

all:	hl2_tcp 

hl2_tcp:	$(FILES)
	$(CC) $(FILES) $(LL) -o hl2_relay

clean:
	rm -f hl2_relay

