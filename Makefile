CC=g++
CFLAGS=-I.
LIBS=-pthread -lavformat -lavutil -lavcodec -lfmt
DEPS=
OBJS=pkt_hub.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

pkt_hub: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f ./*.o *~ core ./include/*~ 
