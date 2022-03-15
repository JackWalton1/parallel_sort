#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

int check_sorted(float* numbers, int size);
void bubblesort(float* numbers, int size);
int synch(int curr_cprocesses,int process_count, int *flags, int next, int last);
void splittasks(int iteration, int process_id, int process_count, int size, int indices[2]);

int main(int argc, char *argv[]){
    int process_count = atoi(argv[1]);
    char file_chars[10];
    float* numbers = (float*)mmap(NULL, sizeof(float)*100000, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    int size = 0;
    int* flags = (int*)mmap(NULL, sizeof(int)*process_count, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    int last = 0;
    int next = 1;
    int curr_cprocesses = 0;
    int pid = 1;

    struct timeval s, e; //for timer

    if(argc!=2){
        fprintf(stderr, "Please run program with 3 arguments:\n \ti.e. ./evenOdd 4 < test1.txt\n");
        return -1;
    }
    // Read in the list of unsorted numbers
    while(scanf("%s", file_chars) > 0){
        numbers[size] = atof(file_chars);
        size++;
    }

    fprintf(stderr, "Unsorted Numbers: ");
    for(int i =0; i<size; i++){//print unsorted array
        fprintf(stderr, "%.2f, ", i, numbers[i]);
    }
    fprintf(stderr, "\n");

    if(process_count>(int)(size/2)){
        process_count = (int)(size/2);
    }
    //fprintf(stderr,"Process count %d\n", process_count);
    //return 0;
    if(process_count==1){//one process does it all
        gettimeofday(&s, NULL);
        bubblesort(numbers, size);
        gettimeofday(&e, NULL);
        fprintf(stderr, "Sorted Numbers: ");
        for(int i =0; i<size; i++){//print sorted array
            fprintf(stderr, "%.2f, ", i, numbers[i]);
        }
        fprintf(stderr, "\n");
        long seconds = (e.tv_sec - s.tv_sec);
        long micros = ((seconds * 1000000) + e.tv_usec) - (s.tv_usec);
        fprintf(stderr, "Time taken for actual sorting (SS.MS):\n\t %ld.%ld\n", seconds, micros);
        munmap(numbers, sizeof(float)*100000);
        munmap(flags, sizeof(int)*process_count);
        return 0;
    }

    do{// depoly children processes to do the sort
        curr_cprocesses++;
        pid = fork();
    }while(curr_cprocesses<process_count && pid!=0);

    int indices[2];//start end
    int iteration = 0;
    gettimeofday(&s, NULL);//start timer
    if(!pid){ //child processes
        //fprintf(stderr,"I am child %d\n", curr_cprocesses);
        while(iteration<size){
            splittasks(iteration, curr_cprocesses, process_count, size, indices);
            iteration++;
            if(indices[0]!=-1){
                for(int ii = indices[0]; ii<indices[1];ii+=2){
                    // fprintf(stderr,"I am child %d with start = %d| end = %d\n", curr_cprocesses, indices[0], indices[1]);
                    if(numbers[ii]>numbers[ii+1]){
                        float temp = numbers[ii];
                        numbers[ii] = numbers[ii+1];
                        numbers[ii+1] = temp;
                    }
                }
            }

            next = synch(curr_cprocesses, process_count, flags, next, last);
            last = next - 1;
        }
        

        return 0;
    }

    for(int k = 0; k<process_count;k++){//wait for all children to finish
        wait(0);
    }

    gettimeofday(&e, NULL);// end timer

    //Printing numbers post-sort and time taken
    fprintf(stderr, "Sorted Numbers: ");
    for(int i =0; i<size; i++){//print sorted array
        fprintf(stderr, "%.2f, ", i, numbers[i]);
    }
    fprintf(stderr, "\n");
    long seconds = (e.tv_sec - s.tv_sec);
    long micros = ((seconds * 1000000) + e.tv_usec) - (s.tv_usec);
    fprintf(stderr, "Time taken for actual sorting (SS.MS):\n\t %ld.%ld\n", seconds, micros);
    munmap(numbers, sizeof(float)*100000);
    munmap(flags, sizeof(int)*process_count);
    return 0;
   
}

void splittasks(int iteration, int process_id, int process_count, int size, int indices[2]){
    int start = 0;
    int end = size -1;
    int accessible = size; //VARIABLE SO SIZE STAYS THE SAME INSIDE THIS FUNC
    int p_number = process_id-1;
    int ceiling; //max number for # indexes per process
    int floor; //min number for # indexes per process
    int ceiling_occurance = 0; //assume we dont need the ceiling
    int floor_occurance = process_count; // assume the floor will work
    int is_odd_iteration = iteration%2; // if odd, start at index 1
    int is_odd_length = size%2;
    if(is_odd_length){
        //fprintf(stderr,"Is odd length\n");
        accessible = accessible -1;
    }
    else if(!is_odd_length && is_odd_iteration){ //is even length and odd iteration
        accessible = accessible - 2; // decrememnt the size that is accessible
    }

    floor = accessible/process_count;
    if(floor%2){
        floor--;
    }
    ceiling = floor + 2;
    while(floor*floor_occurance+ceiling*ceiling_occurance!=accessible&&floor_occurance!=0){
        floor_occurance--;
        ceiling_occurance++;
    }
    if(p_number<floor_occurance){
        start = floor*p_number+is_odd_iteration;
        end = floor*process_id-1+is_odd_iteration;
    }
    else{
        start = floor_occurance*floor+(p_number-floor_occurance)*ceiling+is_odd_iteration;
        end = floor_occurance*floor+(process_id-floor_occurance)*ceiling-1+is_odd_iteration;
    }
        
    indices[0] = start;
    indices[1] = end;

    //CATCH FOR WHEN ON ODD ITERATION ON EVEN LIST AND THERES TOO MANY PROCESSES
        // if you are process one, on an even length array, and there are as many processes as pairs of numbers
    if(process_id==1 && process_count==(int)(size/2) && (!is_odd_length) && is_odd_iteration){
        indices[0] = -1;
        indices[1] = -1;
    }

    return;
}

int synch(int curr_cprocesses,int process_count, int *flags, int next, int last){
    flags[curr_cprocesses-1] = next;
    for(int i = 0; i<process_count; i++){
        while(flags[i]==last){} //wait
    }

    return next+1; // returns the value of next, for next time synch is called
}

void bubblesort(float* numbers, int size){
    int is_sorted = 0;
    int is_odd_length = size%2;
    int compares = (int)(size/2);
    if(check_sorted(numbers,size)){
        return;
    }
    for(int iteration = 0; iteration<size;iteration++){
        if(is_odd_length){
            if(iteration%2!=0){
                for(int j = 0; j<compares;j++){
                    if(numbers[j*2+1]>numbers[j*2+2]){
                        float temp = numbers[j*2+1];
                        numbers[j*2+1] = numbers[j*2+2];
                        numbers[j*2+2] = temp;
                    }
                }

            }
            else{
                for(int j = 0; j<compares;j++){
                    if(numbers[j*2]>numbers[j*2+1]){
                        float temp = numbers[j*2];
                        numbers[j*2] = numbers[j*2+1];
                        numbers[j*2+1] = temp;
                    }
                }
            }

        }else{
            if(iteration%2!=0){
                for(int j = 0; j<compares-1;j++){
                    if(numbers[j*2+1]>numbers[j*2+2]){
                        float temp = numbers[j*2+1];
                        numbers[j*2+1] = numbers[j*2+2];
                        numbers[j*2+2] = temp;
                    }
                }

            }
            else{
                for(int j = 0; j<compares;j++){
                    if(numbers[j*2]>numbers[j*2+1]){
                        float temp = numbers[j*2];
                        numbers[j*2] = numbers[j*2+1];
                        numbers[j*2+1] = temp;
                    }
                }
            }

        }
    }
    return;
}

int check_sorted(float* numbers, int size){
    for(int i = 0; i<size-1;i++){
        //fprintf(stderr, "comparing %.2f with %.2f\n", numbers[i],numbers[i+1]);
        if(numbers[i]>numbers[i+1]){
            return 0;
        }
    }
    return 1;
}