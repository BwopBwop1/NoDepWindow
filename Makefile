.PHONY: all clean run

CFLAGS := -Wall -std=c17 -g -nostdinc -nostdlib -ffreestanding
SOURCES := src/main.c src/start.s

all: build/app

clean:
	rm -rf build

run: build/app
	./build/app

build: 
	mkdir -p build

build/app: build $(SOURCES)
	gcc -o $@ $(SOURCES) $(CFLAGS)
