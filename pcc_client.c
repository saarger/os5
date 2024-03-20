#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

long find_file_length(char* filename)
{
    struct stat st;
    if (stat(filename, &st) < 0){
        perror("failed performing stat()");
    }
    return st.st_size;
}

//reads all file to char* data, and returns it
char* get_file_data(FILE* file, long N)
{
    char* data = (char*) malloc(N);
    if(data==NULL) {
        perror("failed performing malloc");
        exit(1);
    }
    if(fread(data, 1, N, file) != N){
        perror("failed reading the file");
        exit(1);
    }
    return data;
}

// if write_mode==1, this functions writes bytes_num bytes from data_buff to the socket sock_fd
// if write_mode==0, this functions reads bytes_num bytes from the socket sock_fd to data_buff
void transferring_data(int sock_fd, int write_mode, long bytes_num, char* data_buff)
{
    long transferred = 0, not_transferred = bytes_num, cur;
    while(not_transferred > 0){
        cur = write_mode?
                write(sock_fd, data_buff + transferred, not_transferred) :
                read(sock_fd, data_buff + transferred, not_transferred);
        if(cur < 0){
            perror("failed in transferring_data");
            exit(1);
        }
        transferred += cur;
        not_transferred -= cur;
    }
}

int main(int argc, char *argv[])
{
    int sock_fd;
    struct sockaddr_in serv_addr;
    uint32_t N_net_rep, C, C_net_rep, N;
    unsigned short serv_port;
    FILE *file;
    char* file_data;
    if (argc != 4) {
        errno = EINVAL;
        perror("wrong number of command line arguments");
        exit(1);
    }
    //open the file:
    file = fopen(argv[3], "r");
    if(file == NULL) {
        perror("could not open the file");
        exit(1);
    }
    N = find_file_length(argv[3]);
    file_data = get_file_data(file, N);
    if(fclose(file) == EOF) {
        perror("could not close the file");
        exit(1);
    }

    //creat a TCP connection
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("could not creat socket");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_port = (unsigned short) strtol(argv[2], NULL, 10);
    serv_addr.sin_port = htons(serv_port);
    inet_pton(AF_INET, argv[1], &(serv_addr.sin_addr.s_addr));
    if( connect(sock_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
       perror("Connect Failed");
       exit(1);
    }

    //send N to server
    N_net_rep = htonl(N);
    transferring_data(sock_fd, 1, 4, (char *) (&N_net_rep));

    //send the file's content to server
    transferring_data(sock_fd, 1, N, file_data);
    free(file_data);

    //get C from server (number of printable characters)
    transferring_data(sock_fd, 0, 4, (char *) (&C_net_rep));
    C = ntohl(C_net_rep);


    printf("# of printable characters: %u\n", C);
    if(close(sock_fd) < 0) {
        perror("could not close the socket");
        exit(1);
    }
    exit(0);
}
