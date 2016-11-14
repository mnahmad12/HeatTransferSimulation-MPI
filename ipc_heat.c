#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>


int total_procs, proc_id, name_len;
char proc_name[256];

/* Utility to print messages with the proc # prepended to the
   message. Works like printf(). */
void pprintf(const char* format, ...) { 
  va_list args;
  char buffer[2048];
  sprintf(buffer,"P%2d: ",proc_id);
  va_start (args, format);
  vsprintf(&buffer[5], format, args);
  va_end (args);
  printf("%s",buffer);
}

/* Utility to have only the root processor print messages with the
 message. Works like printf(). */
void rprintf(const char* format, ...) { 
  if(proc_id == 0){
    va_list args;
    va_start (args, format);
    vprintf(format, args);
    va_end (args);
  }
}

int main(int argc, char **argv){
	if(argc < 5){
	  printf("usage: %s max_time width print procs\n  max_time: int\n  width: int\n  print: 1 print output, 0 no printing\n #PROCS: int, number of processors to use\n", argv[0]);
		return 0;
  	}
	
	char proc_name[256];
	MPI_Init (&argc, &argv);
	MPI_Comm_rank (MPI_COMM_WORLD, &proc_id);
	MPI_Comm_size (MPI_COMM_WORLD, &total_procs);
	MPI_Get_processor_name(proc_name, &name_len);

	int max_time = atoi(argv[1]); // Number of time steps to simulate
  	int width = atoi(argv[2]);    // Number of cells in the rod
  	int print = atoi(argv[3]);
  	int my_width = width / total_procs;

  	double initial_temp = 50.0;   // Initial temp of internal cells 
  	double L_bound_temp = 20.0;   // Constant temp at Left end of rod
  	double R_bound_temp = 10.0;   // Constant temp at Right end of rod
  	double k = 0.5;               // thermal conductivity constant
        double **H;                   // 2D array of temps at times/locations 
	double **printBuffer;         //Full 2D array which will combine all Hs into one large matrix

	// Allocate memory 
  	H = malloc(sizeof(double*)*max_time); 
  	int t,p;
  	for(t=0;t<max_time;t++)
	  {
	    H[t] = malloc(sizeof(double*)*my_width);
	  }

  	
  	if(proc_id == 0)
	  {
	    
	    //Initialize printBuffer
	    printBuffer = malloc(sizeof(double) * max_time);
	    for(t = 0; t < max_time; t++)
	      {
		printBuffer[t] = malloc(sizeof(double) * width);
	      }
	    
	    // Initialize constant left boundary temperature, first proc
	    for(t=0; t<max_time; t++)
	      {
		H[t][0] = L_bound_temp;
	      }
	    for(p=1; p<my_width;p++)
	      {
		H[0][p] = initial_temp;
	      }	
	  }
	
  	// Initialize constant right boundary temperature, last proc
 	if(proc_id == total_procs-1)
	  {
	    for(t=0; t<max_time; t++)
	      {
		H[t][my_width-1] = R_bound_temp;
	      }
	    for(p=0;p<my_width-1;p++)
	      {
		if(total_procs==1)
		  continue;
		H[0][p] = initial_temp;
	      }
	  }

 	else
	  {
	    // Initialize temperatures at time 0 for internal cells
	    t = 0;
	    for(p=0; p<my_width; p++)
	      {
		H[t][p] = initial_temp;
	      }
	  }
	MPI_Gather(H[0],my_width,MPI_DOUBLE,printBuffer[0],my_width,MPI_DOUBLE,0,MPI_COMM_WORLD);
	// Simulate the temperature changes for cells
	for(t=0; t<max_time-1; t++)
	  {
	    /*
	      @leftValue=double that stores the value RECEIVED from the processor to the left 
	      @rightValue=double that stores the value RECEIVED from the processor to the right
	      @sendLeft=array of 1 double that stores the value TO BE SENT to the processor to the left
	      @sendRight=array of 1 double that stores the value TO BE SENT to the processor to the right
	     
	      Intution: We will first complete the communication of all the "edges" of each processors
	      sub-array and THEN calculate the updated temperatures
	    */

	    //STEP 1: Communication:
	    double leftValue, rightValue;
	    double sendLeft[1], sendRight[1]; 
	    
	    // Send/Receive root processor
	    if(proc_id==0 && total_procs>1)
	      {
		//sendRight=rightmost value in processor's sub-array
		sendRight[0] = H[t][my_width-1];
		MPI_Sendrecv(&sendRight, 1, MPI_DOUBLE, proc_id+1, 1, &rightValue, 1, MPI_DOUBLE, proc_id+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		
	      }

	    // Send/Receive last processor
	    else if(proc_id == total_procs-1 && total_procs>1)
	      {
		sendLeft[0] = H[t][0];
		MPI_Sendrecv(&sendLeft, 1, MPI_DOUBLE, proc_id-1, 1, &leftValue, 1, MPI_DOUBLE, proc_id-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	      }

	    //Send/Receive odd processors
	    else if(proc_id % 2 == 1 && total_procs>1)
	      {
		sendLeft[0] = H[t][0];
		
		sendRight[0] = H[t][my_width-1];
		MPI_Sendrecv(&sendLeft, 1, MPI_DOUBLE, proc_id-1, 1, &leftValue, 1, MPI_DOUBLE, proc_id-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Sendrecv(&sendRight, 1, MPI_DOUBLE, proc_id+1, 1, &rightValue, 1, MPI_DOUBLE, proc_id+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	      }

	    // Send/Receive even processors
	    else if(proc_id % 2 == 0 && total_procs>1)
	      {
		sendLeft[0] = H[t][0];
		sendRight[0] = H[t][my_width-1];
		MPI_Sendrecv(&sendRight, 1, MPI_DOUBLE, proc_id+1, 1, &rightValue, 1, MPI_DOUBLE, proc_id+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Sendrecv(&sendLeft, 1, MPI_DOUBLE, proc_id-1, 1, &leftValue, 1, MPI_DOUBLE, proc_id-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	      }
	    
	    
	    //STEP 2: CALCULATION
	    double left_diff, right_diff, delta;
	    // Calculate next heat temps in internal processors
	    for(p=0; p<my_width;p++)
	      {
		// Skips the left bound delta calculations for root proccessor and skips right bound delta calc for last proccessor	
		if((proc_id == 0 && p == 0) || (proc_id == total_procs-1 && p == my_width-1))
		  continue;
		
		//if border cells need information it uses the leftValue and rightValue info obtained from Sendrecv() call
		//else simply use the value H[t][p-1/p+1]
		
		float diffLeftValue = (p == 0 && total_procs>1) ? leftValue : H[t][p-1];
		float diffRightValue = (p == my_width-1 && total_procs>1) ? rightValue : H[t][p+1];
		
		left_diff = H[t][p] - diffLeftValue;
		right_diff = H[t][p] - diffRightValue;
		delta = -k*(left_diff + right_diff);
		H[t+1][p] = H[t][p] + delta;

	      }
	    //at this point an entire row has been completed, not we will simply ask each processor to send its row sub-section
	    //to processor 0, and have it be stored at the printBuffer (full matrix)
	    MPI_Gather(H[t+1],my_width,MPI_DOUBLE,printBuffer[t+1],my_width,MPI_DOUBLE,0,MPI_COMM_WORLD);
	  }
	
	//PRINTING
	if(print)
	  {
	    //only processor 0 will print since it holds the entire matrix
	    if(proc_id ==0)
	      {
		//printing header
		printf("%3s| ","");
		for(p=0; p<width; p++){
		  printf("%5d ",p);
		}
		printf("\n");

		//printing ___ lines underneath header
		printf("%3s+-","---");
		for(p=0; p<width; p++){
		  printf("------");
		}
		printf("\n");

		/* Row headers and data */
		for(t=0; t<max_time; t++){
		  printf("%3d| ",t);
		  for(p=0; p<width; p++){
		    printf("%5.1f ",printBuffer[t][p]);
		  }
		  printf("\n");
		}
	      }
	  }	
	
	
    	for(t=0; t<max_time; t++)
	{
    		free(H[t]);
  	}
	free(H);
	
	if(proc_id==0)
	  {
	    for(t=0;t<max_time;t++)
	      {
		free(printBuffer[t]);
	      }
	    free(printBuffer);
	  }
	
	MPI_Finalize();

	return 0;
}
      
