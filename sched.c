#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define MAX_PROCS 26
#define RR_QUANTUM 1
#define RR_CAP (MAX_PROCS * 100)

//the states a process can be in
typedef enum {
    NOT_ARRIVED,
    RUNNABLE,
    RUNNING,
    BLOCKED,
    DEAD
} state;

//we keep all the info we need 
typedef struct {
    char name;
    int iotime;
    int first_run;
    int finish_time;
    int runtime;
    int iolength;
    int io_triggered;
    int arrival_time;
    state state;
    int cpu_done;
    int io_timer;
    
    
} proc;

proc ptable[MAX_PROCS]; //process table
int nprocs = 0; //processes we have so far

//we check if every process is dead
int procs_done() {
    for (int i = 0; i < nprocs; i++)
        if (ptable[i].state != DEAD) 
            return 0;
    return 1;
}

//we scan the table to find whoever is currently running
int get_running() {
    for (int i = 0; i< nprocs; i++)
        if (ptable[i].state == RUNNING) 
            return i;
    return -1;
}

//noo we need to simply convert the state into a string so we can print it
const char *state_str(state s) {
    if (s == RUNNABLE) 
        return "Runnable";
    if (s == RUNNING)  
        return "Running";
    if (s == BLOCKED)  
        return "Blocked";
    if (s == DEAD)     
        return "Dead";
    return ""; // not arrived yet, print blank
}


//prints the top row of the table
void table_header() {
    printf("t  ");
    for (int i = 0; i< nprocs; i++)
        printf("| Process %c ", ptable[i].name);
    printf("|\n");
}


//one row for the current tick
void print_tick(int t) {
    printf("%-3d", t); //tick number left aligned
    for (int i = 0; i < nprocs; i++)
        printf("| %-9s ", state_str(ptable[i].state));
    printf("|\n");
}

//FIFO, we know that it just picks whoever got there first, first one who arrived the first one who will be served.
int next_fifo() {
    int best = -1;
    for (int i = 0; i < nprocs; i++) {
        if (ptable[i].state != RUNNABLE) 
            continue;
        if (best == -1 || ptable[i].arrival_time < ptable[best].arrival_time)
            best = i;//Earlier arrival wins
    }
    return best;

}

//SJF, picks the one with shortest runtime
int next_sjf() {
    int best = -1;
    for (int i = 0; i < nprocs; i++) {
        if (ptable[i].state != RUNNABLE) 
            continue;
        if (best == -1 || ptable[i].runtime< ptable[best].runtime)
            best = i;
    }
    return best;
}

//STCF, we look at remaining time, and we include running processes as well
int next_stcf() {
    int best = -1;
    for (int i = 0; i < nprocs; i++) {
        if (ptable[i].state != RUNNABLE && ptable[i].state != RUNNING) 
            continue;
        int rem = ptable[i].runtime - ptable[i].cpu_done;
        if (best == -1) {
            best = i;
        } else {
            int best_rem = ptable[best].runtime - ptable[best].cpu_done;
            if (rem <best_rem) best = i; //less remaining time will win
        }
    }
    return best;
}

void run_sched(int mode) {
    table_header();

    int rr_ticks = 0;
    int rr_ptr = -1; //we use this to remember where round robin left off

    for (int t = 0; !procs_done(); t++) {

        //first we tick down any IO timers
        for (int i = 0; i < nprocs; i++) {
            if (ptable[i].state == BLOCKED) {
                ptable[i].io_timer--; //one tick closer to unblocking
                if (ptable[i].io_timer <= 0)
                    ptable[i].state = RUNNABLE;//IO done, back to runnable
            }
        }

        //then we let new processes arrive, new arrivals
        for (int i = 0; i < nprocs; i++) {
            if (ptable[i].state == NOT_ARRIVED && ptable[i].arrival_time == t)
                ptable[i].state = RUNNABLE;//just arrived
        }

        int runner = get_running();//whoever is on the CPU at this moment

        if (mode == 0) { //FIFO
            if (runner == -1) {
                int next = next_fifo();
                if (next != -1) {
                    ptable[next].state = RUNNING;
                    if (ptable[next].first_run == -1) 
                        ptable[next].first_run = t;//we record first run on CPU
                    runner = next;
                }
            }

        } 
        else if (mode == 1) {  // SJF
            if (runner == -1) {
                int next = next_sjf();
                if (next != -1) {
                    ptable[next].state = RUNNING;
                    if (ptable[next].first_run == -1) ptable[next].first_run = t;
                    runner = next;
                }
            }

        } else if (mode == 2) {// STCF, we check every tick because we know that STCF its preemptive
            int best = next_stcf();
            if (best != -1) {
                if (runner != -1 && runner != best) {
                    ptable[runner].state = RUNNABLE;//kick out current runner
                    ptable[runner].io_triggered = 0;//we reset IO
                }
                if (ptable[best].state == RUNNABLE) {
                    ptable[best].state = RUNNING;
                    if (ptable[best].first_run == -1) 
                        ptable[best].first_run = t;
                }
                runner = best;
            }

        } else { //RR, we scan starting from the last process that ran
            if (runner == -1) {
                for (int k = 1; k <= nprocs; k++) {
                    int idx = (rr_ptr + k) % nprocs;
                    if (ptable[idx].state == RUNNABLE) {
                        ptable[idx].state = RUNNING;
                        if (ptable[idx].first_run == -1) ptable[idx].first_run = t;
                        runner = idx;
                        rr_ptr = idx;//remember where we are right now
                        rr_ticks = 0;//new quantum, a fresh one
                        break;
                    }
                }
            }
        }

    
        
        print_tick(t);

        if (runner == -1) continue;

        ptable[runner].cpu_done++;

        //if we moved past the io checkpoint we reset the flag so it can fire again later
        if (ptable[runner].io_triggered && ptable[runner].iotime > 0 &&
            ptable[runner].cpu_done % ptable[runner].iotime != 0)
            ptable[runner].io_triggered = 0;

        //now, we check if the IO should fire 
        if (!ptable[runner].io_triggered && ptable[runner].iotime > 0 &&
            ptable[runner].cpu_done == ptable[runner].iotime) {
            ptable[runner].state = BLOCKED;
            ptable[runner].io_timer = ptable[runner].iolength + 1; //+1 because timer ticks before we print
            ptable[runner].io_triggered = 1;
            rr_ticks = 0;
            continue;
        }

        //we check if the process is done
        if (ptable[runner].cpu_done == ptable[runner].runtime) {
            ptable[runner].state = DEAD;
            ptable[runner].finish_time = t + 1;
            rr_ticks = 0;
            continue;
        }

        //for round robin we check if the quantum ran out
        if (mode == 3) {
            rr_ticks++;
            if (rr_ticks >= RR_QUANTUM) {
                ptable[runner].state = RUNNABLE;
                rr_ptr = runner;
                rr_ticks = 0;
            }
        }

        //  STCF we also check processes to see if any need to block for IO
        if (mode == 2) {
            for (int i = 0; i < nprocs; i++) {
                if (ptable[i].state != RUNNABLE) continue;
                if (ptable[i].iotime > 0 && !ptable[i].io_triggered && ptable[i].cpu_done > 0 && ptable[i].cpu_done % ptable[i].iotime== 0 && (ptable[i].runtime -ptable[i].cpu_done)>= ptable[i].iotime) {
                    ptable[i].state = BLOCKED;
                    ptable[i].io_timer = ptable[i].iotime;
                    ptable[i].io_triggered = 1;
                }
            }
        }
    }
}

//we read from line by line and parse each process
int load_procs() {
    char line[128];

    while (fgets(line, sizeof(line), stdin)) {
        if (strlen(line) <= 1)//skip blank lines
        continue;

        int a; 
        int b; 
        int c; 
        int d;
        if (sscanf(line, "%d,%d,%d,%d", &a, &b, &c, &d) != 4) {
            printf("Invalid process detected\n");
            return 1;
        }

        ptable[nprocs].name = 'A' + nprocs;
        ptable[nprocs].arrival_time = a;
        ptable[nprocs].runtime  = b;
        ptable[nprocs].iotime   = c;
        ptable[nprocs].iolength  = d;
        ptable[nprocs].state = NOT_ARRIVED;
        ptable[nprocs].cpu_done = 0;
        ptable[nprocs].io_triggered= 0;
        ptable[nprocs].io_timer   = 0;
        ptable[nprocs].first_run = -1;
        ptable[nprocs].finish_time = -1;
        nprocs++;
    }

    if (nprocs == 0) {
        printf("Invalid process detected\n");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int mode = 0; //default is FIFO

    if (argc > 3) {//too many arguments
        printf("sched -m [mode] < [process file]\n");
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if(strcmp(argv[2], "FIFO") == 0) 
                mode = 0;
        else if (strcmp(argv[2], "SJF") == 0) 
                mode = 1;
        else if (strcmp(argv[2], "STCF")== 0) 
                mode = 2;
        else if (strcmp(argv[2], "RR") == 0) 
                mode = 3;
        else {
            printf("sched -m [mode] < [process file]\n");
            return 1;
        }
    }

    if (load_procs()) return 1;
    run_sched(mode);

    double tat = 0; 
    double rt = 0;
    for (int i = 0; i < nprocs; i++) {
        tat += ptable[i].finish_time- ptable[i].arrival_time;
        rt  += ptable[i].first_run - ptable[i].arrival_time;
    }

    printf("Average Turn-around Time: %.3f\n", tat / nprocs);
    printf("Average Response Time: %.3f\n", rt / nprocs);

    return 0;
}
//this project was honestly pretty challenging, and took me longer that expected, specially the IO timer frustrated me a lot, the hardest part for me was getting the IO timing right
// i kept getting the wrong state printed because the timer was decrementing at the wrong time
// once i figured out the +1 trick on io_timer it finally made sense, basically the timer ticks before we print
// the RR scheduler also gave me trouble, i tried using a queue at first but the ordering kept breaking tests
// every time cpu_done hit iotime even after the IO already happened, the modulo reset fixed that
// that is why i struggled tests 7 and 8. i passed 1 through 6 and then spent a lot of time on 7 and 8. every time i changed something to fix test 7, test 5 would break instead
//the issue is that process B shows Runnable in my output where the expected shows Blocked,
//and fixing that IO handling for STCF broke the FIFO test
//test 8 had a similar  problem with RR ordering on large inputs
//but at the end i figured out after days, i was gonna leave at test 6 passing, but figured out what the problem was, removing the io triggered = 0 thats for test 7 and 
//for test 8 the solution was  ditching the queue and scanning forward from rr_ptr instead