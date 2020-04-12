//mandlemovie.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>


void createProcesses(int numProc){
	
	int Jobs = 50;
	int currentJob = 0;
	int runningProc = 0;
	float zoom = 2.0;
	float ZoomStep = exp(log(0.00002/zoom)/51);


	while(currentJob < Jobs){

		if(runningProc < numProc){
			zoom = zoom*ZoomStep;
			runningProc += 1;
			//printf("Increasing Process to %i\n", runningProc);
			currentJob +=1;
			pid_t my_pid = fork();

			if(my_pid < 0){
				fprintf(stderr, "Fork failed\n");
				exit(1);
			}else if(my_pid == 0){
				//printf("There are %i processes running. I am working on job: %i\n", runningProc, currentJob);
				//printf("Zoom Value: %f\n", zoom);
				
				char *my_args[9];
				char buffer[BUFSIZ];
				char buffer2[BUFSIZ];
				
				
				my_args[0] = "./mandel";
				my_args[1] = "-x -0.13";
				my_args[2] = "-y 0.649526";


				sprintf(buffer, "-s %f", zoom);
				my_args[3] = buffer;


				my_args[4] = "-W 1024";
				my_args[5] = "-H 1024";
				my_args[6] = "-m 1000";


				sprintf(buffer2, "-o mandel%i.bmp", currentJob);
				my_args[7] = buffer2;


				my_args[8] = NULL;

				execvp(my_args[0], my_args);

				printf("Should not have made it here: Execvp failed");
				exit(0);
			}else{
				continue;
			}

		}else{
			wait(NULL);
			runningProc-=1;
			//printf("Process Count dropped to %i\n", runningProc);
		}

	}

	while(runningProc > 0){ //Wait on the remaining processes to finish
		wait(NULL);
		runningProc -= 1;
		//printf("Process Count dropeed to %i\n", runningProc);
	}
}

int main(int argc, char* argv[]){

	int numProc = 1;
	if(argc > 1) numProc = atoi(argv[1]);

	createProcesses(numProc);
	
	return 0;
}