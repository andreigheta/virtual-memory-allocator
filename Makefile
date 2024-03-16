# compiler setup
CC=gcc
CFLAGS=-Wall -Wextra -std=c99

# define targets
TARGETS=vma

build: $(TARGETS)

vma: vma.c
	$(CC) $(CFLAGS) vma.c main.c -lm -o vma

pack:
	zip -FSr 312CA_GhetaAndrei_Cristian_Tema1.zip README Makefile *.c *.h

clean:
	rm -f $(TARGETS)

.PHONY: pack clean
