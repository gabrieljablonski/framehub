CC=g++
CFLAGS=-I.
LIBS=-pthread -lavformat -lavutil -lavcodec
DEPS=frame_queue.h frame.h
OBJS=framehub.o frame_queue.o frame.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

framehub: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f ./*.o *~ core ./include/*~ 
