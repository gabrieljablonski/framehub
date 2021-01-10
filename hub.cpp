#include <iostream>
#include <fstream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <libavformat/avformat.h>

#define MAX_CLIENTS 20
struct {
  bool connected;
  bool waiting_send;
} clients[MAX_CLIENTS];

typedef struct {
  int id;
  int local;
  int remote;
  int port;
  bool unique;
  void * (*callback)(void *);
} Socket;

#define DATA_SIZE 10000
static uint8_t data[DATA_SIZE];

void set_waiting() {
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i].connected)
      clients[i].waiting_send = true;
  }
}

bool check_waiting() {
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i].connected && clients[i].waiting_send)
      return true;
  }
  return false;
}

void *producer_handler(void *ptr) {
  std::cerr << "producer connected\n";
  Socket producer = *(Socket *)ptr;
  while (1) {
    if (check_waiting()) continue;
    // std::cerr << "waiting for producer data\n";
    int bytes = recv(producer.remote, &data, DATA_SIZE, MSG_WAITALL);
    set_waiting();
    if (bytes <= 0) {
      std::cerr << "recv failed\n";
      break;
    }
    // std::cerr << "received " << bytes << " bytes from producer\n";
  }
  std::cerr << "producer dropped\n";
  return 0;
}

void *consumer_handler(void *ptr) {
  Socket consumer = *(Socket *)ptr;
  std::cerr << "consumer connected " << consumer.id << std::endl;
  clients[consumer.id].connected = true;
  clients[consumer.id].waiting_send = false;
  while (1) {
    if (!clients[consumer.id].waiting_send)
      continue;
    int bytes = send(consumer.remote, &data, DATA_SIZE, MSG_NOSIGNAL);
    clients[consumer.id].waiting_send = false;
    if (bytes < 0) {
      std::cerr << "send failed\n";
      break;
    }
  }
  clients[consumer.id].connected = false;
  std::cerr << "consumer dropped " << consumer.id << std::endl;
  return 0;
}

void *sock_listen(void *ptr) {
  Socket s = *(Socket *)ptr;

  s.local = socket(AF_INET, SOCK_STREAM , 0);
  if (s.local == -1){
    std::cerr << "producer `socket()` failed.\n";
    exit(-1);
  }

  struct sockaddr_in local_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(s.port),
    .sin_addr = { .s_addr = INADDR_ANY }
  };
  int addr_len = sizeof(local_addr);

  if(bind(s.local, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
    std::cerr << "producer `bind()` failed\n";
    exit(-1);
  }

  listen(s.local, 5);

  while(1) {
    struct sockaddr_in remote_addr;
    s.remote = accept(s.local, (struct sockaddr *)&remote_addr, (socklen_t *)&addr_len);
    if (s.remote < 0) {
        std::cerr << "accept failed\n";
        exit(-1);
    }

    if (s.id != -1) {
      s.id = -1;
      for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i].connected) {
          s.id = i;
          break;
        }
      }
      if (s.id == -1) {
        std::string msg = "max clients reached";
        send(s.remote, &msg, msg.length(), MSG_NOSIGNAL);
        s.id = 0;
        continue;
      }
    }

    pthread_t thread;
    pthread_create(&thread, NULL, s.callback, &s);
    if (s.unique) {
      pthread_join(thread, NULL);
    }
  }
}

int main(int argc, char **argv) {
  int producer_port = 5000;
  int consumers_port = 5001;

  struct sockaddr_in producer_addr, consumers_addr;
          
  if (argc > 1) {
    producer_port = atoi(argv[1]);
  }
  if (argc > 2) {
    consumers_port = atoi(argv[2]);
  }

  std::cout << "listening for producer on " << producer_port << "...\n";
  std::cout << "listening for consumers on " << consumers_port << "...\n";

  Socket p_socket = {
    .id = -1,
    .port = producer_port,
    .unique = true,
    .callback = producer_handler
  };

  Socket c_socket = {
    .port = consumers_port,
    .unique = false,
    .callback = consumer_handler
  };

  pthread_t producer_thread, consumers_thread;

  pthread_create(&producer_thread, NULL, sock_listen, &p_socket);
  pthread_create(&consumers_thread, NULL, sock_listen, &c_socket);

  pthread_join(producer_thread, NULL);
  pthread_join(consumers_thread, NULL);

  return 0;
}
