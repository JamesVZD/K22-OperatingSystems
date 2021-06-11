#ifndef FUNCTIONS_H
#define FUNCTIONS_H          

#include <sys/sem.h>

int isNumerical(char input[]);

int semDown(int semid, int sempos);
int semUp(int semid, int sempos);


union semun{
	int val;
	struct semid_ds *buff;
	unsigned short *array;
};


typedef struct smType{	
	int value;	
	int counter;
	double timestamp;
}SmType;

#endif
