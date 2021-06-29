all: udpfwd

udpfwd: udpfwd.c
	gcc -Wall -o udpfwd udpfwd.c
.PHONY: clean all
clean:
	rm udpfwd