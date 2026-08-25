#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BYTE_FORWARD 0
#define BYTE_REVERSE 1
#define BYTE_STEER_LEFT 2
#define BYTE_STEER_RIGHT 3

int main(int args, char **argv) {

    int fd;
    /* Open the device */
    fd = open("/dev/car_gpio", O_RDWR);
    if (fd == -1) {
        perror("open failed\n");
        printf("errno -> %s\n", strerror(errno));
        exit(-1);
    }
    printf("%s: open: successful\n", argv[0]);

    while (1) {
        char temp_rd[6];
        int n = read(fd, temp_rd, 5);
        if (n < 0) {
            perror("read failed");
            break;
        }

        int fwd = temp_rd[BYTE_FORWARD] - '0';
        int rev = temp_rd[BYTE_REVERSE] - '0';
        char wr_buf[4];
        wr_buf[0] = (!fwd) + '0';
        wr_buf[1] = (!rev) + '0';
        wr_buf[2] = temp_rd[BYTE_STEER_LEFT];
        wr_buf[3] = temp_rd[BYTE_STEER_RIGHT];

        write(fd, wr_buf, 4);
        usleep(100000);
    }

    close(fd);
    return 0;
}
