CFLAGS := -g -Wall

speeders: speeders.o

test: speeders
	nc bilby 30003 | stdbuf -oL speeders

clean:
	rm -f speeders speeders.o