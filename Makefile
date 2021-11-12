CC = clang++
CFLAGS = -Wall -O3 -std=gnu++1z -fno-stack-protector -fno-common
RM = rm -f

ALL_FILE = main.o dyld_extractor

all: dyld_extractor

dyld_extractor: main.o
	$(CC) main.o -o dyld_extractor

main.o: ./DyldExtractor/main.cpp
	$(CC) $(CFLAGS) -c ./DyldExtractor/main.cpp -o main.o

clean:
	$(RM) $(ALL_FILE)
