#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "./locker/locker.h"
#include "./pool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/MinHeapTimer.h"

const int MaxFD = 65536;
const int MaxEventNumber = 10000;
const int TimeSlot = 5;
const int TimeHeapSize = 100;

extern int removefd(int epollfd, int fd);

/* 定时器处理超时连接 */
static int pipefd[2];

static int epollfd = 0;

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void timer_handler(time_heap* timeHeap) {
    timeHeap->tick();
    alarm(TimeSlot);
}

void cb_func(client_data* user_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void setFdLimit() {
    struct rlimit limit;
    limit.rlim_cur = 4096; // 新的软限制
    limit.rlim_max = 4096; // 新的硬限制
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
        perror("setrlimit");
    }
}

void addTimerToHeap(time_heap* timeHeap, int connfd) {
    heap_timer* heapTimer = new heap_timer(20);
    heapTimer->cb_func = cb_func;
    heapTimer->user_data->sockfd = connfd;
    timeHeap->add_timer(heapTimer);
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage : %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    addsig(SIGPIPE, SIG_IGN);

    threadpool<http_conn>* pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    } catch (...) {
        return 1;
    }

    http_conn* users = new http_conn[MaxFD];
    assert(users);
    int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[MaxEventNumber];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);
    addsig(SIGALRM, sig_handler);
    time_heap timeHeap(TimeHeapSize);
    alarm(TimeSlot);

    while (true) {
        int number = epoll_wait(epollfd, events, MaxEventNumber, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address,
                                    &client_addrlength);
                if (connfd < 0) {
                    printf("errno is : %d\n", errno);
                }
                if (http_conn::m_user_count >= MaxFD) {
                    show_error(connfd, "Internal server busy");
                    continue;
                }

                users[connfd].init(connfd, client_address);
                addTimerToHeap(&timeHeap, connfd);
            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret != -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGALRM:
                            {
                                timer_handler(&timeHeap);
                                break;
                            }
                        }
                    }
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockfd].close_conn();
            } else if (events[i].events & EPOLLIN) {
                if (users[sockfd].read()) {
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            } else {}
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}
