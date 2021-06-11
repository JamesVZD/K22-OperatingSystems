#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "functions.h"

int main(int argc, char *argv[]){

	int entries, peers, ratio, reps, mutexid, wrtid, i, pid, smid, iRead, iWrote, j, target;
	unsigned short *arr;
	double u, l, iWaited, wtime, elapsed, avgW;
	Entry *smdata;
	clock_t t;
	union semun arg;
	Entry entry, entry2;

	//Validate Comand Line Input.
	if(argc != 6){
	
		printf("Correct Input: %s <Shared Memory Entries> <Peers Number> <R/W Ratio> <Peer Iterations> <l parameter>\n", argv[0]);
		printf("All arguments, except for the last must be integers. l is a non-zero double. Please try again.\n");
		return 0;
	}
	else if(!isNumerical(argv[1]) || !isNumerical(argv[2]) || !isNumerical(argv[3]) || !isNumerical(argv[4]) || !isNumerical(argv[5])){
		printf("Correct Input: %s <Shared Memory Entries> <Peers Number> <R/W Ratio> <Peer Iterations> <l parameter>\n", argv[0]);
		printf("All arguments, except for the last must be integers. l is a non-zero double. Please try again.\n");
		return 0;
	}
	else{
		entries = atoi(argv[1]);
		peers = atoi(argv[2]);
		ratio = atoi(argv[3]);
		reps = atoi(argv[4]);
		l = atof(argv[5]);
		
		//Prevent Division by 0.
		if(l == 0.0)
			l = 1;
	}
	
	//A k-Ratio means 1 Writer per k Readers <=> 1 Writer in k+1 Peers.
	ratio++; 

	//Create a Shared Memory Segment.
	smid = shmget(IPC_PRIVATE, entries*sizeof(Entry), 0666);
	
	//Attach it to Coordinator and its children.
	smdata = shmat(smid, NULL, 0);



	//Initialise Entries.	
	for(i=0; i<entries; i++)
		smdata[i].readers = 0;
		smdata[i].reads = 0;
		smdata[i].writes = 0;

	//Create 2 Sets of |entries| Semaphores.
	mutexid = semget(IPC_PRIVATE, entries, 0666 | IPC_CREAT);
	wrtid = semget(IPC_PRIVATE, entries, 0666 | IPC_CREAT);			
	
	//Initialise Semaphores on 1.
	arr=malloc(entries*sizeof(int));
	for(i=0; i<entries; i++)
		arr[i]=1;
	arg.array = arr;
	if(semctl(mutexid, 0, SETALL, arg) == -1){
		perror("Failed to initialise Semaphores.\n");
		exit(1);
	}
	if(semctl(wrtid, 0, SETALL, arg) == -1){
		perror("Failed to initialise Semaphores.\n");
		exit(1);
	}
	free(arr);
	
//------------------------------------------Peer Segment------------------------------------------//

	//Create Peers.
	for(i=0; i<peers; i++){
	
		if((pid=fork()) < 0){
			perror("Fork Function Failed.\n");
			exit(1);
		}
		
		//We are in a Child-Peer Process.
		if(pid == 0){
			
			//Initialise Stats.
			iRead = 0;
			iWrote = 0;
			iWaited = 0;
			
			//Initialise Rand()
			srand(time(NULL) + getpid());
			
			for(j=0; j<reps; j++){
			
				//Pick an Entry to Read/Write.
				target = rand() % entries;
				
				//Pick a Random Exponentialy Distributed Time to Stay in Entry.
				u = ((rand() % 1000000) + 1) / 1000000.0; //Avoiding case u=0, when ln(u)=-infinity.
				wtime =0.0001*(-log(u)/l);
				if(wtime > 1.0 || wtime <= 0.0)
					wtime = 1.0;
				
				//Writer Case
				if((rand() % ratio) == 0){
					
					//Writer Critical Section.
					t = clock();
					semDown(wrtid, target);
					t = clock() - t; 
					elapsed = ((double)t)/CLOCKS_PER_SEC;
					iWaited = iWaited + elapsed;
					
					//Incrementing Writes.
					smdata[target].writes++;
					
					//Simulate Action.
					sleep(wtime);
					iWrote++;
					
					//End of Critical Section.
					semUp(wrtid, target);
				}
				
				//Reader Case
				else{
					//Reader Critical Section.
					
					//Reader to Reader Critical Sub-Section.
					t = clock();
					semDown(mutexid, target);
					t = clock() - t; 
					elapsed = ((double)t)/CLOCKS_PER_SEC;
					iWaited = iWaited + elapsed;					
					
					//Incrementing Reader Counter
					smdata[target].readers++;

					//If in First Reader, Block Writers. 
					if(smdata[target].readers == 1){
						t = clock();
						semDown(wrtid, target);
						t = clock() - t; 
						elapsed = ((double)t)/CLOCKS_PER_SEC;
						iWaited = iWaited + elapsed;
					}
					//End of Critical Sub-Section.
					semUp(mutexid, target);
					
					//Simulate Action.
					sleep(wtime);
					iRead++;
					
					//Another Critical Sub-Section.
					t = clock();
					semDown(mutexid, target);
					t = clock() - t; 
					elapsed = ((double)t)/CLOCKS_PER_SEC;
					iWaited = iWaited + elapsed;
					
					//Decrementing Reader Counter and Incrementing Completed Reads.
					smdata[target].reads++;
					smdata[target].readers--;

					
					//If in the Last Reader, Unblock Writers.
					if(smdata[target].readers == 0)
						semUp(wrtid, target);
					
					//End of Critical Sub-Section.
					semUp(mutexid, target);
					
					//End of Critical Section.
					printf("%d\n",j);
				}
			
			}
			
			avgW = iWaited / reps;
			//Print Stats.
			printf("Peer no %d terminating. Reads: %d, Writes: %d, Avg.Waiting Time: %lf\n", i, iRead, iWrote, avgW);
			exit(0);	
		}
	}


//-----------------------------------Coordinator Segment------------------------------------//


	//Coordinator shall wait for all peers to terminate.
	for (i=0; i<peers; i++)
		wait(NULL);
	
	//Print Total Stats.
	putchar('\n');
	for(i=0; i<entries; i++){
	
		printf("Entry %d was read %d times and written %d times.\n", i, smdata[i].reads, smdata[i].writes);
	}
	
	//Delete Semaphores.
	if(semctl(mutexid, 0, IPC_RMID, 0) == -1)
		perror("Failed to delete Semaphores.\n");
	if(semctl(wrtid, 0, IPC_RMID, 0) == -1)
		perror("Failed to delete Semaphores.\n");
	
	//Dettach the Shared Memory Segment.
	shmdt(smdata);
	
	//Free Shared Memory.
	shmctl (smid, IPC_RMID, 0);

return 0;

}
