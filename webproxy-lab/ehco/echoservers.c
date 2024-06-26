#include "../csapp.h"

typedef struct                                                          // 구조체 정의
{
    int maxfd;                                                          // 현재 활성화된 파일 디스크립터의 최댓값임. clientfd 배열의 최대값이 maxfd
    fd_set read_set;                                                    // select 함수를 사용해서 모니터링 할 파일 디스크립터의 집합
    fd_set ready_set;                                                   // 실제로 읽게되는 파일 디스크립터의 집합
    int nready;                                                         // select 함수에 의해 준비된 파일 디스크립터의 수
    int maxi;                                                           // 최댓값의 인덱스
    int clientfd[FD_SETSIZE];                                           // 파일 디스크립터들을 저장하는 배열
    rio_t clientrio[FD_SETSIZE];                                        // 클라이언트와의 통신에 사용되는 rio_t
} pool;

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

int byte_cnt = 0;

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;

    if (argc != 2) {
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);                                  // 포트 열어주기
    init_pool(listenfd, &pool);                                         // pool 초기화

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
        printf("printf done\n");
    }
}


void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}


void add_client(int connfd, pool *p) {
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;                 //line:conc:echoservers:beginaddclient
            Rio_readinitb(&p->clientrio[i], connfd); //line:conc:echoservers:endaddclient

            FD_SET(connfd, &p->read_set); //line:conc:echoservers:addconnfd

            if (connfd > p->maxfd) //line:conc:echoservers:beginmaxfd
                p->maxfd = connfd; //line:conc:echoservers:endmaxfd
            if (i > p->maxi)       //line:conc:echoservers:beginmaxi
                p->maxi = i;       //line:conc:echoservers:endmaxi
            break;
        }
    if (i == FD_SETSIZE) /* Couldn't find an empty slot */
        app_error("add_client error: Too many clients");
}


void check_clients(pool *p) {
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->read_set))) {
            p->nready--;
            printf("p.nready = %d\n", p->nready);
            printf("p.maxi = %d\n", p->maxi);
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                Rio_writen(connfd, buf, n);
            } 

            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}