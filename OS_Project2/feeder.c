#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "functions.h"

int main(int argc, char *argv[]){

	int M, n, mutexid, feedid, readid, pid, smid, *array, i, j ;
	unsigned short *arr, *arf, *arm;
	double t, iWaited, elapsed, avgW;
	SmType *smdata;
	struct timeval tv;
	union semun argr, argf, argm;
	FILE *output;
	
	//Validate Comand Line Input.
	if(argc != 3){
	
		printf("Correct Input: %s <Array Size> <Number of Proccesses>\n", argv[0]);
		return 0;
	}
	else if(!isNumerical(argv[1]) || !isNumerical(argv[2])){
	
		printf("The array must have at least 3000 slots, and there have to be at least 2 processes.\n");
		return 0;
	}
	else{
	
		M = atoi(argv[1]);
		n = atoi(argv[2]);
	}
	
	if(M<3000 || n<0){
	
		printf("The array must have at least 3000 slots, and there have to be at least 2 processes.\n");
		return 0;
	}
	
	//Create a Shared Memory Segment.
	smid = shmget(IPC_PRIVATE, sizeof(SmType), 0666);
	
	//Attach it to Feeder and its children.
	smdata = shmat(smid, NULL, 0);

	//Create 3 Sets of Semaphores.
	mutexid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);	//Memory Access
	feedid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);  //Feeder Access
	readid = semget(IPC_PRIVATE,  n, 0666 | IPC_CREAT);	//New Reader Info
	
	//Initialise Semaphores on 1 for Feeder and Memory Access, and 0 for Readers new info.
	arr=malloc(n*sizeof(int));
	for(i=0; i<n; i++)
		arr[i]=0;
	argr.array = arr;
	if(semctl(readid, 0, SETALL, argr) == -1){
		perror("Failed to initialise Semaphores.\n");
		exit(1);
	}
	
	arf=malloc(1*sizeof(int));
	arf[0]=1;
	argf.array = arf;
	if(semctl(feedid, 0, SETALL, argf) == -1){
		perror("Failed to initialise Semaphores.\n");
		exit(1);
	}
	
	arm=malloc(1*sizeof(int));
	arm[0]=1;
	argm.array = arm;
	if(semctl(mutexid, 0, SETALL, argm) == -1){
		perror("Failed to initialise Semaphores.\n");
		exit(1);
	}
	
	free(arr);
	free(arf);
	free(arm);
	
	//Create the array.
	array=malloc(M*sizeof(int));
	
	//Open output file.
	output = fopen("data.out","a");
	
//------------------------------------------Reader Segment------------------------------------------//

	//Create Readers.
	for(i=0; i<n; i++){
	
		if((pid=fork()) < 0){
			perror("Fork Function Failed.\n");
			exit(1);
		}
		
		//We are in a Child-Reader Process.
		if(pid == 0){
			
			//Initialise Stats.
			iWaited = 0.0;
		
			for(j=0; j<M; j++){
			
				//The i-th reader waits for new value to be available.
				semDown(readid, i);
				
				//When there is new info, reader reaches its Critical Section.
				semDown(mutexid, 0);
				
				array[j] = smdata->value;
				gettimeofday(&tv, NULL);
				t = (tv.tv_sec) * 1000.0 + (tv.tv_usec) / 1000;
				elapsed = t - smdata->timestamp;
				iWaited = iWaited + elapsed;
				smdata->counter++;
				
				//If this is the last reader to access info, allow feeder to proceed.
				if(smdata->counter == n)
					semUp(feedid, 0);
			
				semUp(mutexid, 0);
			
			}

			//Prepare Stats
			pid = getpid();
			avgW = iWaited / M;
			
			//Print stats in orderly fashion.
			semDown(mutexid, 0);
			
			fprintf(stdout, "Reader with pid: %d terminated.\nAverage Waiting Time: %fms\n\n", pid, avgW);
			fprintf(output, "Reader with pid: %d terminated.\nAverage Waiting Time: %fms\nArray Data:\n", pid, avgW);
			for(j=0; j<M; j++)
				fprintf(output, "%d ", array[j]);
			fprintf(output, "\n\n");

			semUp(mutexid, 0);
			
			//Free Memory and exit.
			free(array);
			exit(0);	
		}
	}


//-----------------------------------Feeder Segment------------------------------------//


	srand(time(NULL) + getpid());
	
	//Fill in the Array.
	for(i=0; i<M; i++)
		array[i] = rand() % 100 + 1;
		
	//Feed values one by one.
	for(i=0; i<M; i++){
	
		//Wait for all readers to access previous info.
		semDown(feedid, 0);
	
		//Then fill the shared memomy with the next value.
		smdata->value = array[i];
		smdata->counter = 0;
		gettimeofday(&tv, NULL);
		smdata->timestamp = (tv.tv_sec) * 1000.0 + (tv.tv_usec) / 1000;
		
		//And allow readers to access the new info.
		for(j=0; j<n; j++)
			semUp(readid, j);
	}

	//Wait for all readers to terminate.
	for (i=0; i<n; i++)
		wait(NULL);

	//Delete Semaphores.
	if(semctl(mutexid, 0, IPC_RMID, 0) == -1)
		perror("Failed to delete Semaphores.\n");
	if(semctl(feedid, 0, IPC_RMID, 0) == -1)
		perror("Failed to delete Semaphores.\n");
	if(semctl(readid, 0, IPC_RMID, 0) == -1)
		perror("Failed to delete Semaphores.\n");
	
	//Free Array.
	free(array);
	
	//Close Output File.
	fclose(output);
	
	//Dettach the Shared Memory Segment.
	shmdt(smdata);
	
	//Free Shared Memory.
	shmctl (smid, IPC_RMID, 0);

return 0;

}
