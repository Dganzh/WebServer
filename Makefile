SOURCE = utils.c server.c
OBJECT_SERVER = utils.o server.o
all:
	gcc -c -g3 $(SOURCE)
	gcc -g3 -o server $(OBJECT_SERVER) -lpthread

.PHONY: clean
clean:
	-rm server $(OBJECT_SERVER)

run:
	./server