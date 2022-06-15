CC := cc
CFLAGS := -O3 -Wall
LDLIBS := -lm

OBJS := speeders.o castotas.o

speeders: $(OBJS)

test: speeders
	nc bilby 30003 | stdbuf -oL speeders -b | stdbuf -oL tee test.log

clean:
	rm -f speeders $(OBJS) test.log
