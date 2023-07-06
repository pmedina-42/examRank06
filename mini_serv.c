#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <sys/socket.h>

fd_set afds, wfds, rfds;

int count = 0;
int max_fd = 0;
char read_buff[1001];
char write_buff[42];
int ids[65536];
char *msgs[65536];

void fatal_error() {
	write(2, "Fatal error\n", 12);
	exit(1);
}

char *str_join(char *s1, char *s2) {
	char *str;
	int len = 0;

	if (s1 != 0)
		len = strlen(s1);
	str = malloc(sizeof(*str) * (len + strlen(s2) + 1));
	if (str == 0)
		fatal_error();
	str[0] = 0;
	if (s1 != 0)
		strcat(str, s1);
	free(s1);
	strcat(str, s2);
	return (str);
}

void notify_clients(int sender, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &wfds) && fd != sender)
			send(fd, msg, strlen(msg), 0);
	}
}

void receive_client(int fd) {
	max_fd = fd > max_fd ? fd : max_fd;
	ids[fd] = count++;
	msgs[fd] = 0;
	FD_SET(fd, &afds);
	sprintf(write_buff, "server: client %d just arrived\n", ids[fd]);
	notify_clients(fd, write_buff);
}

void remove_client(int fd) {
	sprintf(write_buff, "server: client %d just left\n", ids[fd]);
	notify_clients(fd, write_buff);
	free(msgs[fd]);
	FD_CLR(fd, &afds);
	close(fd);
}

int create_socket() {
	max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (max_fd < 0)
		fatal_error();
	FD_SET(max_fd, &afds);
	return (max_fd);
}

int extract_msg(char **buff, char **msg) {
	char *newbuff;
	int i = 0;

	if (*buff == 0)
		return (0);
	*msg = 0;
	while ((*buff)[i]) {
		if ((*buff)[i] == '\n') {
			newbuff = calloc(1, sizeof(*newbuff) * (strlen(*buff + i + 1) + 1));
			if (newbuff == 0)
				fatal_error();
			strcpy(newbuff, *buff + i + 1);
			*msg = *buff;
			(*msg)[i + 1] = 0;
			*buff = newbuff;
			return (1);
		}
		i++;
	}
	return (0);
}

void send_msg(int fd) {
	char *msg = NULL;

	while (extract_msg(&msgs[fd], &msg)) {	
		sprintf(write_buff, "client %d: ", ids[fd]);
		notify_clients(fd, write_buff);
		notify_clients(fd, msg);
		free(msg);
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	FD_ZERO(&afds);

	int port = atoi(argv[1]);
	int socket_fd = create_socket();
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = 0x0100007f;
	servaddr.sin_port = ((port & 0x00ff) << 8) + ((port & 0xff00) >> 8);

	if (bind(socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		fatal_error();
	if (listen(socket_fd, SOMAXCONN))
		fatal_error();
	while (42) {
		rfds = wfds = afds;
		if (select(max_fd + 1, &rfds, &wfds, 0, 0) < 0)
			fatal_error();
		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &rfds))
				continue;
			if (fd == socket_fd) {
				socklen_t addr_len = sizeof(servaddr);
				int client_fd = accept(fd, (struct sockaddr *)&servaddr, &addr_len);
				if (client_fd >= 0)
					receive_client(client_fd);
			} else {
				int bytes_len = recv(fd, read_buff, 1000, 0);
				if (bytes_len <= 0) {
					remove_client(fd);
					break;
				}
				read_buff[bytes_len] = '\0';
				msgs[fd] = str_join(msgs[fd], read_buff);
				send_msg(fd);
			}
		}
	}
	return (0);
}
