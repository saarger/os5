#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

int conn_fd = 0;
int got_sigint = 0;
uint32_t pcc_total[95] = {0};

void print_pcc_total()
{
    for(int i = 0; i < 95; i++){
        printf("char '%c' : %u times\n", i+32, pcc_total[i]);
    }
}

void my_signal_handler(int signum)
{
    if (conn_fd == 0){
        //no client is on process
        print_pcc_total();
        exit(0);
    }
    else {
        //turn on this flag, to exit the loop after handling the current client
        got_sigint = 1;
    }
}

int register_signal_handling()
{
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = my_signal_handler;
    return sigaction(SIGINT, &new_action, NULL);
}

//returns the number of printable characters in file_data, and also updating pcc_total
uint32_t update_pcc(const char* file_data, uint32_t N)
{
    uint32_t C = 0;
    char chr;
    for(int i = 0; i < N; i++){
        chr = file_data[i];
        if(chr >= 32 && chr <= 126){
            C++;
            pcc_total[chr-32]++;
        }
    }
    return C;
}

// if write_mode==1, this functions writes to the socket sock_fd the data in data_buff
// (first bytes_num bytes of it);
// if write_mode==0, this functions reads bytes_num bytes from the socket sock_fd to data_buff
// it returns 1 iff we encountered a TCP error
int transferring_data(int sock_fd, int write_mode, long bytes_num, char* data_buff)
{
    long transferred = 0, not_transferred = bytes_num, cur;
    while(not_transferred > 0){
        cur = write_mode?
              write(sock_fd, data_buff + transferred, not_transferred) :
              read(sock_fd, data_buff + transferred, not_transferred);
        if(cur < 0) {
            if (!(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE)) {
                perror("failed in transferring_data");
                exit(1);
            }
            else {
                return 1; // indicates that we encountered TCP error
            }
        }
        if(cur == 0){ //client process killed unexpectedly
            return 1; // indicates that we encountered TCP error
        }
        transferred += cur;
        not_transferred -= cur;
    }
    return 0; // no TCP error
}

int main(int argc, char *argv[])
{
    if (register_signal_handling() == -1){
        perror("register_signal_handling failed");
        exit(1);
    }

    unsigned short serv_port;
    int listen_fd, encountered_TCP_error;
    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    uint32_t N, N_net_rep, C_net_rep, cur_C;

    if (argc != 2){
        errno = EINVAL;
        perror("wrong number of command line arguments");
        exit(1);
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1) {
        perror("failed performing socket");
        exit(1);
    }

    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("setsockopt failed");
        exit(1);
    }

    memset( &serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_port = (unsigned short) strtol(argv[1], NULL, 10);
    serv_addr.sin_port = htons(serv_port);

    if( 0 != bind(listen_fd, (struct sockaddr*) &serv_addr, addrsize ))
    {
        perror("bind failed");
        exit(1);
    }

    if( 0 != listen(listen_fd, 10))
    {
        perror("listen failed");
        exit(1);
    }

    while(!got_sigint){
        conn_fd = accept( listen_fd, (struct sockaddr*) &peer_addr, &addrsize);
        if(conn_fd == -1) {
            perror("failed performing accept");
            exit(1);
        }

        //read N from client
        transferring_data(conn_fd, 0, 4,  (char *) (&N_net_rep));
        N = ntohl(N_net_rep);

        char* file_data = (char*) malloc(N);
        if(file_data==NULL) {
            perror("failed performing malloc");
            exit(1);
        }
        //read file's content from client
        encountered_TCP_error = transferring_data(conn_fd, 0, N, file_data);
        if(encountered_TCP_error){
            perror("encounterred TCP error");
        }
        else{
            cur_C = update_pcc(file_data, N);

            //write C to client
            C_net_rep = htonl(cur_C);
            transferring_data(conn_fd, 1, 4, (char *) (&C_net_rep));
        }
        free(file_data);
        conn_fd = close(conn_fd);
        if(conn_fd < 0) {
            perror("could not close the socket");
            exit(1);
        }
    }

    print_pcc_total();
    if(close(listen_fd) < 0) {
        perror("could not close the socket");
        exit(1);
    }
    exit(0);

}