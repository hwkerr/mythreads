//threadtest.c

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mythreads.h"


int value = 0;

void *t1 (void *arg)
{
	int param = *((int*)arg);
	printf("t1 started %d\n",param);

	//threadYield();

	threadLock(0);
	value++;
	printf("+1\n");
	threadYield();
	if (param > 10) {
		threadYield();
		value += 5;
		printf("+5\n");
		threadWait(0, 0);
	}
	else {
		value -= 5;
		printf("-5\n");
		threadSignal(0, 0);
	}
	threadUnlock(0);
	//threadYield();

	int* result = malloc(sizeof(int));
	*result = param + 1;
	printf ("added 1! (%d)\n",*result);

	threadLock(0);
	threadLock(1);
	value += 100;
	printf("+100\n");
	threadUnlock(0);
	threadUnlock(1);

	threadYield();

	printf("t1: done result=%d\n",*result);
	return result;
}



int main(void)
{
	int id1, id2;
	int p1;
	int p2;

	p1 = 23;
	p2 = 2;

	int *result1, *result2;

	value = 0;

	//initialize the threading library. DON'T call this more than once!!!
	threadInit();

	id1 = threadCreate(t1,(void*)&p1);
	printf("created thread 1.\n");

	id2 = threadCreate(t1,(void*)&p2);
	printf("created thread 2.\n");



	threadJoin(id1, (void*)&result1);
	printf("joined #1 --> %d.\n",*result1);

	threadJoin(id2, (void*)&result2);
	printf("joined #2 --> %d.\n",*result2);

	assert(value == 202);
}
