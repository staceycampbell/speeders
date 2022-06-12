CC := gcc
CFLAGS := -O2 -Wall
LDLIBS := -lm

speeders: speeders.o

test: speeders
	nc bilby 30003 | stdbuf -oL speeders -b | stdbuf -oL tee test.log

clean:
	rm -f speeders speeders.o test.log
