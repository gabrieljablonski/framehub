CC=g++
CFLAGS=-I./include
LIBS=-pthread
DEPS=
OBJS=hub.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

hub: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f ./*.o *~ core ./include/*~ 
