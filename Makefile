CC := cc
CFLAGS := -I/usr/include/libxml2 -g -mtune=native -Wall
LDLIBS := -lm -lcurl -lxml2

OBJS := speeders.o castotas.o

all: speeders tb

speeders: $(OBJS)

tb: tb.o metar.o

test: speeders
	nc bilby 30003 | stdbuf -oL speeders -b | stdbuf -oL tee test.log

clean:
	rm -f speeders $(OBJS) test.log

.PHONY: all clean test
