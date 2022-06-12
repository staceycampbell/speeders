CC := gcc
CFLAGS := -O2 -Wall
LDLIBS := -lm

speeders: speeders.o

test: speeders
	nc localhost 30003 | stdbuf -oL speeders | stdbuf -oL tee test.log

clean:
	rm -f speeders speeders.o test.log
