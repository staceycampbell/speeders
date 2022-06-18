CC := cc
CFLAGS := -I/usr/include/libxml2 -O3 -mtune=native -Wall -Wno-dangling-else
LDLIBS := -lm -lcurl -lxml2

OBJS := speeders.o castotas.o metar.o

all: speeders tb

speeders: $(OBJS)

tb: tb.o metar.o castotas.o

test: speeders
	nc bilby 30003 | stdbuf -oL speeders -b | stdbuf -oL tee test.log

clean:
	rm -f speeders tb tb.o $(OBJS) test.log

.PHONY: all clean test
