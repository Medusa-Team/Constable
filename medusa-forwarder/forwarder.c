/**
 * @file forwarder.c
 * @brief TCP forwarder of Medusa DS9 requests.
 *
 * (c) 2002 Milan Pikula
 * $Id: forwarder.c,v 1.4 2002/10/17 11:09:38 www Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

static void mylog(int fd, char * banner, char * buf, int len)
{
	struct timeval tv;
	char msg[1024];
	int j;

	gettimeofday(&tv, NULL);
	snprintf (msg, sizeof(msg),
		"%s: %d bytes, %ld.%ld sec", banner, len,
		tv.tv_sec, tv.tv_usec);
	write(fd, msg, strlen(msg));
	for (j=0; j<len; j++) {
		sprintf(msg, "%s%.2x",
			(j%16 == 0) ? "\n\t" : " ",
			(unsigned char)buf[j]);
		write(fd, msg, strlen(msg));
	}
	write (fd, "\n", 1);
}

int main(int argc, char * argv[])
{
	struct hostent * he;
	struct sockaddr_in si;
	int sock, dev;
	char buf[512];
	fd_set in;
	int i, j, tmp;

	int log_fd;

	if (argc < 3 || argc > 4) {
		printf("Usage: forwarder <hostname> <port> [<logfile>]\n");
		return 1;
	}
	if (argc == 4)
		log_fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC);
	else
		log_fd = -1;
	he = gethostbyname(argv[1]);
	if (he)
		si.sin_addr.s_addr = *((unsigned int *) he->h_addr);
	si.sin_port = htons(atoi(argv[2]));
	si.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Cannot create network socket");
		return -1;
	}
	if (connect(sock, (struct sockaddr *)&si, sizeof(si)) == -1) {
		perror("Cannot create network connection");
		return -1;
	}
	dev = open("/dev/medusa", O_RDWR);
	if (dev == -1) {
		perror("Cannot open Medusa DS9 device");
		return -1;
	}
	while (1) {
		FD_ZERO(&in);
		FD_SET(sock, &in);
		FD_SET(dev, &in);
		select(dev>sock ? dev+1 : sock+1,
			&in, NULL, NULL, NULL);
		if (FD_ISSET(sock, &in)) {
			i = read(sock, buf, sizeof(buf));
			if (i<=0) {
				close(dev);
				return 0;
			}
			if (log_fd != -1)
				mylog(log_fd, "N->K", buf, i);
			j = i;
			while (j) {
				tmp = write(dev, buf + i-j, j);
				if (tmp < 0) {
					close(dev);
					return 0;
				}
				j -= tmp;
			}
		}
		if (FD_ISSET(dev, &in)) {
			i = read(dev, buf, sizeof(buf));
			if (i<=0) {
				close(dev);
				return 0;
			}
			if (log_fd != -1)
				mylog(log_fd, "K->N", buf, i);
			j = i;
			while (j) {
				tmp = write(sock, buf + i-j, j);
				if (tmp < 0) {
					close(dev);
					return 0;
				}
				j -= tmp;
			}
		}
	}
}
