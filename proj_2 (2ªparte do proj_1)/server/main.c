#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <bits/types/sigset_t.h>
#include <errno.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

pthread_t workerThreads[MAX_SESSION_COUNT];
int sessionIDs[2][MAX_RESERVATION_SIZE] = {0};
int print_info_flag = 0;

typedef struct {
    int session_id;
    char req_pipe_path[40];
    char resp_pipe_path[40];
    pthread_t worker_thread;
} SessionInfo;

// Define a structure for the buffer
typedef struct {
    SessionInfo data[MAX_SESSION_COUNT];  // MAX_BUFFER_SIZE is the maximum number of sessions the buffer can hold
    int front;
    int rear, count;
} SessionBuffer;

SessionInfo sessions[MAX_SESSION_COUNT];
int active_clients = 0;
pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;


// Initialize the buffer
SessionBuffer sessionBuffer;
// Initialize the buffer
void initializeBuffer() {
    sessionBuffer.front = 0;
    sessionBuffer.rear = -1;
    sessionBuffer.count = 0;
}

// Function to check if the buffer is full
int isBufferFull(const SessionBuffer *buffer) {
    return buffer->count == MAX_SESSION_COUNT;
}

// Function to add a session to the buffer
void enqueueSession(SessionBuffer *buffer, SessionInfo session) {
    if (isBufferFull(buffer)) {
        return;
    }

    buffer->rear = (buffer->rear + 1) % MAX_SESSION_COUNT;
    buffer->data[buffer->rear] = session;
    buffer->count++;
}

// Function to remove a session from the buffer
SessionInfo dequeueSession(SessionBuffer *buffer) {

    SessionInfo dequeuedSession = buffer->data[buffer->front];
    buffer->front = (buffer->front + 1) % MAX_SESSION_COUNT;
    buffer->count--;

    return dequeuedSession;
}

void sigusr1_handler() {
    // Memorize that the main thread should print information about events.
    print_info_flag = 1;
}

void *worker_thread_function() {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  while (1){
    pthread_mutex_lock(&session_mutex);
    pthread_cond_wait(&cond, &session_mutex);
    pthread_mutex_unlock(&session_mutex);

    //sessionIDs[0][thread_index] = 1;
    SessionInfo currentSession = dequeueSession(&sessionBuffer);

    
    int req_pipe_fd = open(currentSession.req_pipe_path, O_RDONLY); 
    int resp_pipe_fd = open(currentSession.resp_pipe_path, O_WRONLY);
    if (req_pipe_fd == -1 || resp_pipe_fd == -1) {
          perror("Error opening pipes");
          // Handle error
          return 0;
    }


    // Send the session_id back to the client
    ssize_t bytes_written;
    bytes_written = write(resp_pipe_fd, &currentSession.session_id, sizeof(currentSession.session_id));
    if (bytes_written != sizeof(currentSession.session_id)) {
      // Handle error;
      perror("Error writing session_id to response pipe");
    }

    while(1){ //le os pedidos do cliente 

      char request_buffer[pipeBuffer];
      memset(request_buffer, 0, sizeof(request_buffer));
      read(req_pipe_fd, &request_buffer, sizeof(request_buffer));  
      char command;
      if (sscanf(request_buffer, " %c", &command) == 1) {
        if (command == '2'){
          sessionIDs[0][currentSession.session_id] = 0;
          break;
        }

        // Successfully read the command
        switch (command) {
          case '2':
          break;

          case '3':
            // Command 3 stands for create event
            unsigned int create_event_id;
            size_t create_num_rows, create_num_cols;
            if (sscanf(request_buffer, " %c %u %zu %zu", &command, &create_event_id, &create_num_rows, &create_num_cols) == 4) {
                // Store session information
            }
            int answerC = ems_create(create_event_id, create_num_rows, create_num_cols);
            write(resp_pipe_fd, &answerC, sizeof(int));
            
            break;
          case '4':
            // Command 4 stands for reserve event
            size_t request_buffer_size = strlen(request_buffer) + 1;
            char ReserveBuFe[pipeBuffer];
            char auxBuf[pipeBuffer];
            strncpy(ReserveBuFe, request_buffer, request_buffer_size);
            unsigned int reserve_event_id;
            size_t reserve_num_seats;
            size_t *reserve_xs, *reserve_ys;
            reserve_xs = (size_t*)malloc(MAX_RESERVATION_SIZE * sizeof(size_t));
            reserve_ys = (size_t*)malloc(MAX_RESERVATION_SIZE * sizeof(size_t));
            // Assuming MAX_RESERVATION_SIZE is the maximum number of elements in reserve_xs and reserve_ys
            if (sscanf(ReserveBuFe, " %c %u %zu", &command, &reserve_event_id, &reserve_num_seats) == 3) {
              
            }

            sprintf(auxBuf, "%c %u %zu", command, reserve_event_id, reserve_num_seats);

            int upgrade = 0;

            for (size_t i = 0; i < reserve_num_seats; ++i) {
                if (sscanf(ReserveBuFe + strlen(auxBuf) + 1 + upgrade, " %zu %zu",&reserve_xs[i], &reserve_ys[i]) != 2) {
                    // Handle parsing error
                    break;
                }

                char aux2Buf[pipeBuffer];
                sprintf(aux2Buf, "%zu %zu", reserve_xs[i], reserve_ys[i]);
                upgrade += (int) strlen(aux2Buf) + 1;
                memset(aux2Buf, 0, strlen(aux2Buf));
            }

            int answer = ems_reserve(reserve_event_id, reserve_num_seats, reserve_xs, reserve_ys);
            
            write(resp_pipe_fd, &answer, sizeof(int));
            free(reserve_xs);
            free(reserve_ys);
            break;

          case '5':
            unsigned int show_event_id;
            if (sscanf(request_buffer, " %c %u", &command, &show_event_id) == 2) {
                // Store session information
            }
            ems_show(resp_pipe_fd, show_event_id);
            
            break;

          case '6':
            if (sscanf(request_buffer, " %c", &command) == 1) {
                // Store session information
            }
            ems_list_events(resp_pipe_fd);
            
            break;

          default:
            break;
        }
      }
    }
    if (active_clients > MAX_SESSION_COUNT)
      active_clients = MAX_SESSION_COUNT;
    if (active_clients == (MAX_SESSION_COUNT)){
      pthread_cond_signal(&buffer_not_full);
    }
    active_clients = active_clients - 1;
  }
 }

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }
  // Create the named pipe
  if (mkfifo(argv[1], 0666) == -1) {
      perror("Error creating named pipe");
      return 1;
  }
  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  // Open the named pipe for reading
  int pipe_fd = open(argv[1], O_RDWR);
  if (pipe_fd == -1) {
      perror("Error opening named pipe");
      ems_terminate();
      return 1;
  }

  initializeBuffer();

  if(signal(SIGUSR1, sigusr1_handler) == SIG_ERR){
    exit(EXIT_FAILURE);
  }

  //TODO: Intialize server, create worker threads
  for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
    sessionIDs[1][i] = i;
    pthread_create(&workerThreads[i], NULL, worker_thread_function, &sessionIDs[1][i]);
  }

  while (1) {
    //TODO: Read from pipe
      char buffer[pipeBuffer];
      memset(buffer, 0, sizeof(buffer));
      //while (strlen(buffer) == 0){
        if(print_info_flag == 1){
          ems_program_status();
          print_info_flag = 0;
          if(signal(SIGUSR1, sigusr1_handler) == SIG_ERR){
            exit(EXIT_FAILURE);
          }
        }
        ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
          if(errno == EINTR){
            continue;
          }
          else{
            perror("Error reading from pipe");
            break; //EINTR
          }
        //} 
      }
      if (active_clients == (MAX_SESSION_COUNT)){
        pthread_mutex_lock(&server_mutex);
        pthread_cond_wait(&buffer_not_full, &server_mutex);
        pthread_mutex_unlock(&server_mutex);
      }

      pthread_mutex_lock(&session_mutex);

      SessionInfo new_session;
      if (active_clients > MAX_SESSION_COUNT)
        active_clients = MAX_SESSION_COUNT;
      active_clients++;
      memset(new_session.req_pipe_path, '\0', sizeof(new_session.req_pipe_path));
      memset(new_session.resp_pipe_path, '\0', sizeof(new_session.resp_pipe_path));

      if (sscanf(buffer, "%s %s", new_session.req_pipe_path, new_session.resp_pipe_path) == 2){
        // Store session information
      }

      for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if(sessionIDs[0][i] == 0){
          new_session.session_id = i;
          sessionIDs[0][i] = 1;
          break;
        }
      }
      //TODO: Write new client to the producer-consumer buffer
      enqueueSession(&sessionBuffer, new_session);
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&session_mutex);

      memset(buffer, 0, sizeof(buffer));
                                                                                                                                                                                                                                 
  }
  //TODO: Close Server
  for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        int result = pthread_join(workerThreads[i], NULL);
        if (result != 0) {
            perror("Thread join failed");
            return 1;
        }
        printf("Thread %d joined successfully\n", i);
  }
  
  ems_terminate();
  return 0;
}