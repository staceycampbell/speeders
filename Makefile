CC := cc
CFLAGS := -I/usr/include/libxml2 -O3 -mtune=native -Wall -Wno-dangling-else
LDLIBS := -lm -lcurl -lxml2

OBJS := castotas.o metar.o datetoepoch.o

all: speeders tb

speeders: speeders.o $(OBJS)

tb: tb.o $(OBJS)

test: speeders
	nc localhost 30003 | stdbuf -oL speeders -b | stdbuf -oL tee test.log

clean:
	rm -f speeders speeders.o tb tb.o $(OBJS) test.log

.PHONY: all clean test
