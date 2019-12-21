CC=gcc
CFLAGS=-Wall -g

BINS=clean libmythreads
TEST=cooperative_test preemtive_test test1 test2
all: $(BINS)
test: $(TEST)

libmythreads:
	$(CC) $(CFLAGS) -c mythreads.c mythreads.h
	ar -cvrs libmythreads.a mythreads.o

cooperative_test:
	$(CC) $(CFLAGS) -o cooperative_test cooperative_test.c libmythreads.a

preemtive_test:
	$(CC) $(CFLAGS) -o preemtive_test preemtive_test.c libmythreads.a

test1:
	$(CC) $(CFLAGS) -o test1 test1.c libmythreads.a

test2:
	$(CC) $(CFLAGS) -o test2 test2.c libmythreads.a

clean:
	rm -rf $(BINS) *.o *.dSYM *.a $(TEST)
