CC=g++
CFLAGS=-I.
LIBS=-pthread -lavformat -lavutil -lavcodec
DEPS=
OBJS=frame_hub.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

frame_hub: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f ./*.o *~ core ./include/*~ 
