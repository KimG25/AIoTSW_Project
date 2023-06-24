/*
웹 서버 실행 시 준비해야 할 것
1. sudo apt-get install apache2
2. sudo a2enmod cgi
3. /usr/lib/cgi-bin/tmp 디렉토리 생성
4. sudo chmod 666 /dev/gpiomem

FND Device 연결 방법
- lab 2-4 ppt 8번 슬라이드처럼 핀을 연결

웹 서버 실행 방법
1. .cgi 파일 빌드하기
1-1. gcc -o start.cgi stopwatch.c -lwiringPi -pthread
1-2. gcc -o stop.cgi stop.c
1-3. gcc -o clear.cgi clear.c
2. sudo cp start.cgi stop.cgi clear.cgi /usr/lib/cgi-bin (stopwatch.html은 /var/www/html에 옮겨야 함 : sudo mv stopwatch.html /var/www/html)
3. sudo chown root:root /usr/lib/cgi-bin/start.cgi
4. sudo chown root:root /usr/lib/cgi-bin/stop.cgi
5. sudo chown root:root /usr/lib/cgi-bin/clear.cgi
6. sudo chmod 777 /usr/lib/cgi-bin/* (stopwatch.html도 권한이 필요할 수 있으므로 확인 필요)
7. sudo service apache2 restart

웹 서버 접속 및 스톱워치 실행 방법
1. 라즈베리파이 IP 주소로 접속 ex) http://192.168.0.130/stopwatch.html 이 때 접속은 라즈베리 파이에 할당된 DHCP IP
2. start 버튼을 눌러 스톱워치를 시작 -> 처음 실행 시 fifo 파일이 make됨
3. stop 버튼을 눌러 스톱워치를 멈춤
4. clear 버튼을 눌러 스톱워치를 초기화 -> 이 때 fifo 파일이 삭제됨, 스레드 종료


** 만약 스톱워치가 실행되고 있을 때 apache2 restart를 하게 되면 수동으로 rm -rf /usr/lib/cgi-bin/temp/fifo 파일을 삭제시켜야 함.

comment : 제 FND Monitor가 고장이 났는지 잘 모르겠는데 자꾸 10초정도 지나면 0번째 자리가 꺼지는 현상이 발생합니다. 버그인지 아닌지 확실하지가 않는 상황입니다.
        그래서 FND Monitor 출력 자리를 1번부터 5번까지로 설정했는데 만약에 0번째 자리가 꺼지지 않는다면 0번째 자리도 출력하도록 만들어 주셔도 될 것 같습니다. 
        안 하셔도 무관

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wiringPi.h>
#include <string.h>

#define FIFO_PATH "/usr/lib/cgi-bin/tmp/fifo"   // FIFO 파일 경로

const int FndSelectPin[6] = {4, 17, 18, 27, 22, 23};
const int FndPin[8] = {6, 12, 13, 16, 19, 20, 26, 21};
const int FndFont[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                         0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static int status;

pthread_t fndThreadId, fifoThreadId;

//Do not edit
void FndSelect(int position) {
    int i;

    for (i = 0; i < 6; i++) {
        if (i == position) {
            digitalWrite(FndSelectPin[i], LOW);
        } else {
            digitalWrite(FndSelectPin[i], HIGH);
        }
    }
}

//Do not edit
void FndDisplay(int position, int num) {
    int i;
    int flag = 0;
    int shift = 0x01;

    for (i = 0; i < 8; i++) {
        flag = (FndFont[num] & shift);
        if (i == 7 && (position == 2 || position == 4)) {
            flag |= 0x80;
        }
        digitalWrite(FndPin[i], flag);
        shift <<= 1;
    }
}

// FND를 제어하는 스레드 함수
void* fndThread(void* arg) {
    int pos;
    int data[6] = {0, 0, 0, 0, 0, 0}; // FND에 출력할 값
    unsigned long time = millis(); //시작 시간
    int i = 0;
    int isStopped = 0; //멈춰 있는지
    unsigned long prevStopped = 0; //멈추기 시작한 시간

    while (1) {
        if (status == -1) { //status : clear
            break;
        } else if (status == 0) { //status : stop
            if(isStopped == 0){ //멈춘다는 신호를 처음 받는다면
                isStopped = 1;
                prevStopped = millis(); //멈추기 시작한 시점을 기록
            }
        } else if(status == 1) { //status : start
            if(isStopped == 1){ //멈춘 뒤 다시 시작이라면
                isStopped = 0;
                time += millis() - prevStopped; //멈춘 시간 동안을 더해줌
            }

            // 밀리 초로부터 스톱 워치의 자리수 계산하여 data 값에 저장
            unsigned int elapsed = millis() - time; // 경과 시간 계산
            unsigned int minutes = elapsed / 60000; // 분 단위 계산
            unsigned int seconds = (elapsed / 1000) % 60; // 초 단위 계산
            unsigned int milliseconds = (elapsed % 1000) / 10; // 밀리초 단위 계산
            data[5] = minutes / 10; //십의 자리 분
            data[4] = minutes % 10; // 일의 자리 분
            data[3] = seconds / 10; // 십의 자리 초
            data[2] = seconds % 10; // 일의 자리 초
            data[1] = milliseconds / 10; // 십의 자리 밀리초
            data[0] = milliseconds % 10; // 일의 자리 밀리초

            if(minutes == 10){ //오버 플로우 시 스레드 종료
                break;
            }

        }
        //FND Monitor Display
        for (pos = 0; pos < 6; pos++) {
            FndSelect(pos);
            FndDisplay(pos, data[pos]);
            delay(1);
        }
    }

    //Thread 종료 시 Fifo 파일 삭제
    int result = remove(FIFO_PATH);
    if (result != 0){
        perror("Failed to delete FIFO file");
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

// FIFO로 IPC를 수행하는 스레드 함수
void* fifoThread(void* arg) {
    int fd;
    char message[256];

    // FIFO 열기
    while(1){
        if ((fd = open(FIFO_PATH, O_RDONLY)) < 0) {
            perror("Failed to open FIFO");
            pthread_exit(NULL);
        }
        if (read(fd, message, sizeof(message)) < 0) {
            perror("Failed to read from FIFO");
            close(fd);
            pthread_exit(NULL);
        }
        if (strcmp(message, "stop ") == 0){
            status = 0;
        }else if (strcmp(message, "reset") == 0){
            status = -1;
            break;
        }else if (strcmp(message, "start") == 0){
            status = 1;
        }
        close(fd);
    }

    // FIFO 닫기
    close(fd);

    pthread_exit(NULL);
}

//Do not edit
void Init() {
    int i;

    if (wiringPiSetupGpio() == -1) {
        printf("Setup Fail");
        exit(1);
    }

    for (i = 0; i < 6; i++) {
        pinMode(FndSelectPin[i], OUTPUT);
        digitalWrite(FndSelectPin[i], HIGH);
    }

    for (i = 0; i < 8; i++) {
        pinMode(FndPin[i], OUTPUT);
        digitalWrite(FndPin[i], LOW);
    }
}

int main() {
    
    pid_t pid;
    int fd;

    umask(0);

    //FIFO 파일이 없으면 생성 -> 처음 실행할 때만 실행됨 = 처음 실행할 때만 fork() 실행
    if (access(FIFO_PATH, F_OK) == -1) {
        if (mkfifo(FIFO_PATH, 0666) == -1) {
            perror("Failed to create FIFO");
            return EXIT_FAILURE;
        }
        pid = fork();
    } else {
        pid = -1;
    }

    //Redirect to /stopwatch.html
    printf("Content-type: text/html\n\n");
    printf("<script>window.location.href = '/stopwatch.html';</script>\n");

    if (pid == -1) { // 2번 이상 실행 시 이쪽으로 
        fd = open(FIFO_PATH, O_WRONLY | O_TRUNC);
        if (write(fd, "start", strlen("start")) < 0) {
            perror("Failed to write to FIFO");
            close(fd);
            return EXIT_FAILURE;
        }
        close(fd);
        return EXIT_SUCCESS;
    } else if (pid == 0) { //처음 실행 시 시작되는 자식 process

        Init();
        status = 1;
        pthread_create(&fndThreadId, NULL, fndThread, NULL);
        pthread_create(&fifoThreadId, NULL, fifoThread, NULL);
        pthread_join(fifoThreadId, NULL);

        fd = open(FIFO_PATH, O_WRONLY | O_TRUNC);
        if (write(fd, "start", strlen("start")) < 0) {
            perror("Failed to write to FIFO");
            close(fd);
            return EXIT_FAILURE;
        }
        close(fd);
    }
    return EXIT_SUCCESS;
}
