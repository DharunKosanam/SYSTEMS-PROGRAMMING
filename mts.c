#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#define MAX_TRAINS 75
#define MAX_WAITING 100

typedef struct {
    int id;              
    char direction;      
    int priority;        
    int loading_time;    
    int crossing_time;   
    double ready_time;   
    pthread_cond_t cond; 
    int scheduled;      
} Train;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sched_cond = PTHREAD_COND_INITIALIZER;
Train *waiting_list[MAX_WAITING];
int waiting_count = 0;
int finished_count = 0;
int total_trains = 0;
int track_in_use = 0; 
// 0: free, 1: occupied

char last_direction = '\0'; 
// last train's crossing direction
int consecutive_count = 0;  
// consecutive trains that crossed in same direction
struct timeval start_time;   // simulation start time

// this function rounds elapsed time to the nearest 10th of a sec.
void get_sim_time_str(char *buffer, size_t buf_size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double elapsed = (tv.tv_sec - start_time.tv_sec) +
                     (tv.tv_usec - start_time.tv_usec) / 1000000.0;
    int total_tenths = (int)(elapsed * 10 + 0.5); // rounding to nearest 10th
    int hours = total_tenths / 36000;
    int minutes = (total_tenths % 36000) / 600;
    int seconds = (total_tenths % 600) / 10;
    int tenths = total_tenths % 10;
    snprintf(buffer, buf_size, "%02d:%02d:%02d.%d", hours, minutes, seconds, tenths);
}

// this Remove a train from the waiting list.
void remove_train_from_waiting(Train *t) {
    for (int i = 0; i < waiting_count; i++) {
        if (waiting_list[i] == t) {
            for (int j = i; j < waiting_count - 1; j++)
                waiting_list[j] = waiting_list[j + 1]; // shift the train left
            waiting_count--;
            break;
        }
    }
}

// Find the best candidate from waiting list based on scheduling rules.
Train *find_best_candidate() {
    Train *candidate = NULL;
    int filter = 0; // if set, consider only trains going opposite of last_direction
    if (consecutive_count >= 2 && last_direction != '\0') {
        for (int i = 0; i < waiting_count; i++) {
            if (waiting_list[i]->direction != last_direction) {
                filter = 1;
                break;
            }
        }
    }
    for (int i = 0; i < waiting_count; i++) {
        Train *t = waiting_list[i];
        if (filter && t->direction == last_direction)
            continue; // skip trains in same direction if opposite exists
        if (candidate == NULL)
            candidate = t;
        else {
            if (t->priority > candidate->priority)
                candidate = t;
            else if (t->priority == candidate->priority) {
                if (t->direction == candidate->direction) {
                    if (t->ready_time < candidate->ready_time ||
                       (t->ready_time == candidate->ready_time && t->id < candidate->id))
                        candidate = t;
                } else {
                    if (last_direction == '\0') {
                        if (t->direction == 'W' && candidate->direction != 'W')
                            candidate = t;
                        else if (t->direction == candidate->direction) {
                            if (t->ready_time < candidate->ready_time ||
                               (t->ready_time == candidate->ready_time && t->id < candidate->id))
                                candidate = t;
                        }
                    } else {
                        if (candidate->direction == last_direction && t->direction != last_direction)
                            candidate = t;
                        else if (candidate->direction != last_direction && t->direction != last_direction) {
                            if (t->ready_time < candidate->ready_time ||
                                (t->ready_time == candidate->ready_time && t->id < candidate->id))
                                candidate = t;
                        }
                    }
                }
            }
        }
    }
    // If no candidate found due to filtering, choose from all waiting trains.
    if (candidate == NULL && waiting_count > 0) {
        candidate = waiting_list[0];
        for (int i = 1; i < waiting_count; i++) {
            Train *t = waiting_list[i];
            if (t->priority > candidate->priority)
                candidate = t;
            else if (t->priority == candidate->priority) {
                if (t->direction == candidate->direction) {
                    if (t->ready_time < candidate->ready_time ||
                       (t->ready_time == candidate->ready_time && t->id < candidate->id))
                        candidate = t;
                } else {
                    if (last_direction == '\0') {
                        if (t->direction == 'W' && candidate->direction != 'W')
                            candidate = t;
                        else if (t->direction == candidate->direction) {
                            if (t->ready_time < candidate->ready_time ||
                               (t->ready_time == candidate->ready_time && t->id < candidate->id))
                                candidate = t;
                        }
                    } else {
                        if (candidate->direction == last_direction && t->direction != last_direction)
                            candidate = t;
                        else if (candidate->direction != last_direction && t->direction != last_direction) {
                            if (t->ready_time < candidate->ready_time ||
                               (t->ready_time == candidate->ready_time && t->id < candidate->id))
                                candidate = t;
                        }
                    }
                }
            }
        }
    }
    return candidate;
}

// Scheduler thread: assigns the main track to the next waiting trains.
void *scheduler_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mutex);
    while (finished_count < total_trains) {
        while (track_in_use || waiting_count == 0) {
            if (finished_count == total_trains) {
                pthread_mutex_unlock(&mutex);
                return NULL;
            }
            pthread_cond_wait(&sched_cond, &mutex);
        }
        Train *candidate = find_best_candidate();
        if (candidate == NULL) {
            pthread_mutex_unlock(&mutex);
            continue;
        }
        remove_train_from_waiting(candidate);
        candidate->scheduled = 1;
        track_in_use = 1;              // reserve track
        pthread_cond_signal(&candidate->cond); // signal train to cross
        pthread_mutex_unlock(&mutex);
        pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// Train thread: this simulates loading, waiting, crossing, and finishing.
void *train_thread(void *arg) {
    Train *t = (Train *)arg;
    usleep(t->loading_time * 100000); // simulate loading time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t->ready_time = (tv.tv_sec - start_time.tv_sec) +
                    (tv.tv_usec - start_time.tv_usec) / 1000000.0;
    char time_str[16];
    get_sim_time_str(time_str, sizeof(time_str));
    
    pthread_mutex_lock(&mutex);
    printf("%s Train %2d is ready to go %4s\n", time_str, t->id,
           (t->direction == 'E') ? "East" : "West");
    waiting_list[waiting_count++] = t;    // adds to waiting list
    pthread_cond_signal(&sched_cond);       // notify scheduler
    while (!t->scheduled)
        pthread_cond_wait(&t->cond, &mutex); // waitng until scheduled
    if (last_direction == t->direction)
        consecutive_count++;
    else
        consecutive_count = 1;
    last_direction = t->direction;
    get_sim_time_str(time_str, sizeof(time_str));
    printf("%s Train %2d is ON the main track going %4s\n", time_str, t->id,
           (t->direction == 'E') ? "East" : "West");
    pthread_mutex_unlock(&mutex);
    
    usleep(t->crossing_time * 100000); // simulate crossing time
    
    pthread_mutex_lock(&mutex);
    get_sim_time_str(time_str, sizeof(time_str));
    printf("%s Train %2d is OFF the main track after going %4s\n", time_str, t->id,
           (t->direction == 'E') ? "East" : "West");
    finished_count++;
    track_in_use = 0;              // frees the track
    pthread_cond_signal(&sched_cond); // notifying scheduler that track is free
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }
    Train *trains[MAX_TRAINS];
    total_trains = 0;
    char dir_char;
    int load, cross;
    while (fscanf(fp, " %c %d %d", &dir_char, &load, &cross) == 3) {
        if (total_trains >= MAX_TRAINS)
            break;
        Train *t = malloc(sizeof(Train));
        t->id = total_trains;
        t->direction = (dir_char == 'e' || dir_char == 'E') ? 'E' : 'W';
        t->priority = isupper(dir_char) ? 1 : 0;
        t->loading_time = load;
        t->crossing_time = cross;
        t->scheduled = 0;
        pthread_cond_init(&t->cond, NULL);
        trains[total_trains++] = t;
    }
    fclose(fp);
    
    gettimeofday(&start_time, NULL); // record simulation start time
    pthread_t scheduler;
    pthread_create(&scheduler, NULL, scheduler_thread, NULL); // create scheduler thread
    
    pthread_t train_threads[MAX_TRAINS];
    for (int i = 0; i < total_trains; i++)
        pthread_create(&train_threads[i], NULL, train_thread, (void *)trains[i]); // create train threads
    
    for (int i = 0; i < total_trains; i++)
        pthread_join(train_threads[i], NULL); // wait for all train threads
    pthread_join(scheduler, NULL);              // wait for scheduler thread
    
    for (int i = 0; i < total_trains; i++) {
        pthread_cond_destroy(&trains[i]->cond);
        free(trains[i]);
    }
    
    return 0;
}
