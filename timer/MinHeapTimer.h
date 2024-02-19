#ifndef MIN_HEAP
#define MIN_HEAP

#include <netinet/in.h>
#include <time.h>

#define BUFFER_SIZE 64

class heap_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer* timer;
};

class heap_timer {
public:
    heap_timer (int delay) {
        expire = time(NULL) + delay;
    }
public:
    time_t expire;
    void (*cb_func)(client_data*);
    client_data* user_data;
};

class time_heap {
public:
    time_heap(int cap);
    time_heap(heap_timer** init_array, int size, int capacity);
    ~time_heap();

public:
    void add_timer(heap_timer* timer);

    void del_timer(heap_timer* timer);

    heap_timer* top() const;

    void pop_timer();

    void tick();

    bool empty() const {return cur_size == 0;}

private:
    void percolate_down(int hole);

    void resize();

private:
    heap_timer** array;
    int capacity;
    int cur_size;
};

#endif
