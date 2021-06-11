#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/sem.h>


int isNumerical(char input[]){

	int i;

	for(i=0; input[i]!='\0'; i++)
		if(isdigit(input[i] == 0))
			return 0;
	return 1;
}



int semDown(int semid, int sempos){

	struct sembuf buf[1];		//Info Buffer for semop().
	
	buf[0].sem_num = sempos;	//Semaphore Position.
	buf[0].sem_op = -1;		//Decrement Semaphore.
	buf[0].sem_flg = 0;		//Flag Set to Zero as Instructed in Lab.
	
	if(semop(semid, buf, 1) == -1){	//Buf is an "Array" of a Single Struct, so its Size is 1. 
		perror("Failed to Down Semaphore.\n");
		return 1;
	}
	return 0;
}



int semUp(int semid, int sempos){

	struct sembuf buf[1];
	
	buf[0].sem_num = sempos;	
	buf[0].sem_op = 1;
	buf[0].sem_flg = 0;
	
	if(semop(semid, buf, 1) == -1){
		perror("Failed to Up Semaphore.\n");
		return 1;
	}
	return 0;
}
