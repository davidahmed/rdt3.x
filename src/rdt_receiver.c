/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "packet.h"
#include "common.h"
#include <sys/time.h>



/*
 * You ar required to change the implementation to support
 * window size greater than one.
 * In the currenlt implemenetation window size is one, hence we have
 * onlyt one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;

int main(int argc, char **argv) {
    //int r;

    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    int lastAcked = 0; //the seqno.of last acked packet.
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    while (1) {
        //r = rand() % 30;
        
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }

        //if (r < 3){  VLOG(DEBUG, "SKIP");continue;}
        recvpkt = (tcp_packet *) buffer;

        if ( (recvpkt->hdr.data_size == 0) && (recvpkt->hdr.seqno == lastAcked)) {
            //VLOG(INFO, "End Of File has been reached");
            fclose(fp);
            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = 0;
            if (sendto(sockfd, sndpkt, sizeof(sndpkt), 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
            }

            break;
        }

 
        if ( (lastAcked>0) && (recvpkt->hdr.seqno != lastAcked)){ //.Out of order packet.
            //VLOG(DEBUG, "DoubleACK: seqno: %d and lastAcked: %d ",
                //recvpkt->hdr.seqno, lastAcked);

            //. resend the last in order packet.
            if (sendto(sockfd, sndpkt, sizeof(sndpkt), 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }
            continue;
        }
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu.%lu,%d,%d",tp.tv_sec,tp.tv_usec*1000,recvpkt->hdr.data_size, recvpkt->hdr.seqno);

        fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
        fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
        sndpkt = make_packet(0);
        sndpkt->hdr.ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
        sndpkt->hdr.ctr_flags = ACK;
        //VLOG(DEBUG, "current lastAcked %d", lastAcked);
        lastAcked = sndpkt->hdr.ackno;

        if (sendto(sockfd, sndpkt, sizeof(sndpkt), 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }
        
    }

    return 0;
}
