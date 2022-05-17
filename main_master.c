#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <wiringPi.h>
#include <stdint.h>

#include "settings.h"

#define SPEAKER_PIN 7

struct Client {
    char ID;
    char connected;
    char ack;
    char time_diff_flag;
    long round_time;
    uint32_t time_diff;
};

void playAudio() {
	for(int i = 0; i < 5; i++) {
        digitalWrite(SPEAKER_PIN, 1);
        delay(5);
        digitalWrite(SPEAKER_PIN, 0);
	    delay(5);
	}
}


int main() {

    struct Client clientlist[CLIENTCNT] = { {0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
                                            {0x02, 0x00, 0x00, 0x00, 0x00, 0x00},
                                            {0x03, 0x00, 0x00, 0x00, 0x00, 0x00} };

    // Init GPIO
    if(wiringPiSetup() == -1) {
        return 1;
    }
    pinMode(SPEAKER_PIN, OUTPUT);

    int sockfd;
    char buffer[REC_BUFF_SIZE];
    char msg[SEND_BUFF_SIZE] = {0x00};
    struct sockaddr_in servaddr, cliaddr;
    
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    
    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    
    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    socklen_t len = sizeof(cliaddr);
    int n = 0;

    #if !DEBUG

        // Wait for all 3 clients to connect & say hello
        while(clientlist[0].connected == 0x00)// || client_list[1][1] == 0x00 || client_list[2][1] == 0x00) // TODO: uncomment
        {
            n = recvfrom(sockfd, (char *)buffer, REC_BUFF_SIZE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);

            printf("Server received: ");
            for (int i = 0; i < REC_BUFF_SIZE; i++) {
                printf("%x ", buffer[i]);
            }
            printf("\n");

            for (int i = 0; i < CLIENTCNT; i++) {
                if(buffer[0] == (clientlist[i]).ID << 4 | 0x01) {
                    clientlist[i].connected = 0x01;
                }
            }
        }

        printf("All 3 clients are connected\n");

    #endif

    struct timeval t1, t2;

    // Start measurement routine
    while(1){

        // Start roundtrip measurement       
        msg[0] = 0x01;
        while (clientlist[0].ack == 0x00) //|| c2.ack == 0x00 || c3.ack == 0x00) //TODO: uncomment
        {
            gettimeofday(&t1, NULL);
            sendto(sockfd, (const char *)msg, SEND_BUFF_SIZE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
            n = recvfrom(sockfd, (char *)buffer, REC_BUFF_SIZE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
            gettimeofday(&t2, NULL);
            printf("test %s\n", buffer);

            for (int i = 0; i < CLIENTCNT; i++) {
                if (buffer[0] == clientlist[i].ID << 4 | 0x02) {
                    clientlist[i].round_time = (t2.tv_sec * 1E6 + t2.tv_usec) - (t1.tv_sec * 1E6 + t1.tv_usec);
                    clientlist[i].ack = 0x01;
                }
            }

            // Add delay to avoid receiving old messages from other clients in next loop- cycle??
        }

        printf("acks received\n");

        // Broadcast signal to start measurement
        msg[0] = 0x02;
        sendto(sockfd, (const char *)msg, SEND_BUFF_SIZE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);

        // Wait X seconds
        sleep(2);

        // Play audio
        playAudio();

        // If received -> Calc results; if not -> Display error
        //float speedOfSound = 343.2; // 343.2 m/s
        while(clientlist[0].time_diff_flag != 0) {//(c1.time_diff != 0) { // || c2.time_diff != 0 || c3.time_diff != 0

            n = recvfrom(sockfd, (char *)buffer, REC_BUFF_SIZE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);

            for (int i = 0; i < CLIENTCNT; i++) {

                if (buffer[0] == clientlist[i].ID) {

                    for (int i = 1; i < REC_BUFF_SIZE; i++) {
                        clientlist[i].time_diff << (REC_BUFF_SIZE-i);
                    }
                }

                printf("Time difference of client %d is: %d\n", clientlist[i].ID, clientlist[i].time_diff);
            }

        }

        // calc results

        // print results

        // reset time_diff_flag & time_diff for next loop cycle
        for (int i = 0; i < REC_BUFF_SIZE; i++) {
            clientlist[i].time_diff_flag = 0x00;
            clientlist[i].time_diff = 0x00;
        }
    }
    
    return 0;
}
