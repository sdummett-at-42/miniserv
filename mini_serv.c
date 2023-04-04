#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

// clang -Wall -Wextra -Werror mini_serv.c
// ./a.out 8081

typedef struct s_client {
	int id;
	int fd;
	char *buf;
	struct s_client *next;
} t_client;

t_client *clients = NULL;
fd_set readset, writeset, current;
int sockfd, g_id;
char gigabuf[42 * 4096 * 42] = {0};

void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	exit(1);
}

void send_to_all(int fd) {
	t_client *tmp = clients;

	while (tmp) {
		if (tmp->fd != fd && FD_ISSET(tmp->fd, &writeset))
			send(tmp->fd, gigabuf, strlen(gigabuf), 0);
		tmp = tmp->next;
	}
	bzero(gigabuf, 42 * 4096 * 42);
}

void add_client() {
	t_client *tmp = clients;
	t_client *new = NULL;
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);

	int connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd == -1)
		fatal();
	new = malloc(sizeof(t_client));
	if (!new)
		fatal();
	new->fd = connfd;
	new->id = g_id++;
	new->buf = NULL;
	new->next = NULL;
	FD_SET(new->fd, &current);
	sprintf(gigabuf, "server: client %d just arrived\n", new->id);
	send_to_all(new->fd);
	if (clients == NULL) {
		clients = new;
		return;
	}
	while (tmp->next)
		tmp = tmp->next;
	tmp->next = new;
}

void remove_client(int fd) {
	t_client *tmp = clients;
	t_client *to_del = NULL;

	if (clients && clients->fd == fd) {
		to_del = clients;
		clients = clients->next;
	}
	else {
		while (tmp && tmp->next) {
			if (tmp->next->fd == fd) {
				to_del = tmp->next;
				tmp->next = tmp->next->next;
				break;
			}
			tmp = tmp->next;
		}
	}
	if (to_del) {
		sprintf(gigabuf, "server: client %d just left\n", to_del->id);
		send_to_all(to_del->fd);
		free(to_del->buf);
		free(to_del);
	}
	FD_CLR(fd, &current);
	close(fd);
}

int get_max_fd() {
	int max_fd = sockfd;
	t_client *tmp = clients;

	while (tmp) {
		if (tmp->fd > max_fd)
			max_fd = tmp->fd;
		tmp = tmp->next;
	}
	return max_fd;
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int main(int ac, char **av) {
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		fatal();
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	if (listen(sockfd, 1024) != 0)
		fatal();

	FD_ZERO(&current);
	FD_SET(sockfd, &current);
	while (1) {
		readset = writeset = current;
		if (select(get_max_fd() + 1, &readset, &writeset, NULL, NULL) == -1)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &readset)) {
				if (fd == sockfd) {
					add_client();
					break;
				}
				t_client *tmp = clients;
				while (tmp && tmp->fd != fd)
					tmp = tmp->next;
				if (!tmp)
					break;
				char buf[1024] = {0};
				int retrecv = recv(tmp->fd, buf, 1023, 0);
				if (retrecv <= 0) {
					remove_client(tmp->fd);
					break;
				}
				tmp->buf = str_join(tmp->buf, buf);
				char *msg = NULL;
				while (extract_message(&tmp->buf, &msg)) {
					sprintf(gigabuf, "client %d: %s", tmp->id, msg);
					send_to_all(tmp->fd);
					free(msg);
				}
			}
		}
	}
	return 0;
}
