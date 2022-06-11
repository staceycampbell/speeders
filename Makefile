CFLAGS := -g -Wall
LDLIBS := -lm

speeders: speeders.o

test: speeders
	nc bilby 30003 | stdbuf -oL speeders

clean:
	rm -f speeders speeders.o
