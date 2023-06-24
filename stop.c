#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define FIFO_PATH "/usr/lib/cgi-bin/tmp/fifo"  // FIFO 파일 경로

int main() {
    int fd;
    char message[] = "stop ";

    // FIFO 열기
    if ((fd = open(FIFO_PATH, O_WRONLY | O_TRUNC)) < 0) {
        perror("Failed to open FIFO");
        printf("Content-type: text/html\n\n");
        printf("<script>window.location.href = '/stopwatch.html';</script>\n");
        return EXIT_FAILURE;
    }

    // FIFO에 메시지 전송
    if (write(fd, message, strlen(message)) < 0) {
        perror("Failed to write to FIFO");
        close(fd);
        printf("Content-type: text/html\n\n");
        printf("<script>window.location.href = '/stopwatch.html';</script>\n");
        return EXIT_FAILURE;
    }

    // FIFO 닫기
    close(fd);

    printf("Content-type: text/html\n\n");
    printf("<script>window.location.href = '/stopwatch.html';</script>\n");

    return EXIT_SUCCESS;
}
