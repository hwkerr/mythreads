//threadtest.c

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mythreads.h"


int value = 0;

void *t1 (void *arg)
{
	int param = *((int*)arg);
	//printf("t1 started %d\n",param);

	int* result = malloc(sizeof(int));
	*result = param + 1;
	//printf ("added 1! (%d)\n",*result);

	//threadYield();

	//printf("t1: done result=%d\n",*result);
	return result;
}



int main(int argc, char **argv)
{
	int thread_count = 500;
	if (argc >= 2)
		thread_count = atoi(argv[1]);

	int id1, id2;
	int p1;
	int p2;

	p1 = 23;
	p2 = 2;

	int p[thread_count];

	int *result1, *result2;

	value = 0;

	//initialize the threading library. DON'T call this more than once!!!
	threadInit();

	id1 = threadCreate(t1,(void*)&p1);
	printf("created thread 1.\n");

	id2 = threadCreate(t1,(void*)&p2);
	printf("created thread 2.\n");

	int i;
	for (i = 1; i < thread_count; i++)
	{
		p[i] = i;
		threadCreate(t1,(void*)&p[i]);
	}

	for (i = 1; i < thread_count; i++)
	{
		threadJoin(i, NULL);
	}

	threadJoin(id1, (void*)&result1);
	printf("joined #1 --> %d.\n",*result1);

	threadJoin(id2, (void*)&result2);
	printf("joined #2 --> %d.\n",*result2);
}
