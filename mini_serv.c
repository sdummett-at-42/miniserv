#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client {
	int id;
	int fd;
	char *msg;
	struct s_client *next;
} t_client;

fd_set current, read_set, write_set;
t_client *clients = NULL;
int sockfd, g_id;
char giga_buf[42 * 4096 * 42];

int extract_message(char **buf, char **msg) {
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i]) {
		if ((*buf)[i] == '\n') {
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

char *str_join(char *buf, char *add) {
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

int get_max_fd() {
	t_client *tmp = clients;
	int max_fd = sockfd;

	while (tmp) {
		if (tmp->fd > max_fd)
			max_fd = tmp->fd;
		tmp = tmp->next;
	}
	return max_fd;
}

int get_id(int fd) {
	t_client *tmp = clients;

	while (tmp) {
		if (tmp->fd == fd)
			return tmp->id;
		tmp = tmp->next;
	}
	return 42;
}

void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sockfd);
	exit(1);
}

void send_to_all(int fd) {
	t_client *tmp = clients;

	while (tmp) {
		if (FD_ISSET(tmp->fd, &write_set) && tmp->fd != fd)
			if (send(tmp->fd, giga_buf, strlen(giga_buf), 0) < 0)
				fatal();
		tmp = tmp->next;
	}
	bzero(giga_buf, 42 * 4096 * 42);
}

void add_client() {
	t_client			*tmp = clients;
	struct sockaddr_in	clientaddr;
	socklen_t			len = sizeof(clientaddr);

	int client = accept(sockfd, (struct sockaddr *)&clientaddr, (socklen_t *)&len);
	if (client == -1)
		fatal();

	sprintf(giga_buf, "server: client %d just arived\n", g_id);
	send_to_all(client);
	FD_SET(client, &current);
	t_client *new = malloc(sizeof(t_client));
	if (!new)
		fatal();
	new->fd = client;
	new->id = g_id++;
	new->msg = NULL;
	new->next = NULL;
	if (!tmp)
		clients = new;
	else {
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
}

void rm_client(int fd) {
	t_client	*tmp = clients;
	t_client	*to_del = NULL;

	if (clients && clients->fd  == fd) {
		to_del = clients;
		clients = clients->next;
	}
	else {
		while (tmp && tmp->next && tmp->next->fd != fd)
			tmp = tmp->next;
		if (tmp && tmp->next && tmp->next->fd == fd) {
			to_del = tmp->next;
			tmp->next = tmp->next->next;
		}
	}
	if (to_del) {
		sprintf(giga_buf, "server: client %d just left\n", to_del->id);
		send_to_all(to_del->fd);
		free(to_del);
	}
	FD_CLR(fd, &current);
	close(fd);
}

int main(int ac, char **av) {
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
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
		read_set = write_set = current;
		if (select(get_max_fd() + 1, &read_set, NULL, NULL, NULL) == -1)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &read_set)) {
				if (fd == sockfd) {
					add_client();
					break;
				}
				t_client *tmp = clients;
				int ret = 1;
				while (tmp && tmp->fd != fd)
					tmp = tmp->next;
				if (tmp) {
					char buf[1024] = {0};
					ret = recv(tmp->fd, buf, 1023, 0);
					tmp->msg = str_join(tmp->msg, buf);
				}
				if (ret <= 0) {
					rm_client(fd);
					break;
				}
				char *msg = NULL;
				while (extract_message(&(tmp->msg), &msg)) {
					sprintf(giga_buf, "client %d: %s", tmp->id, msg);
					send_to_all(tmp->fd);
					free(msg);
				}
			} /* if (FD_ISSET(fd, &read_set)) */
		} /* for loop */
	} /* while 1 */
	return 0;
}