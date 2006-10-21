
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#define gdb_printf printf

unsigned int program_pid = 1;

int gdb_socket;

int gdb_client_socket;

enum {
	GDB_NOT_INITIALIZED,
	GDB_INITIALIZED,
	GDB_CONNECTED
} gdb_state;

struct sockaddr_in gdb_addr;

unsigned char gdb_cmd_buffer[256];

unsigned char *gdb_cmd_ptr;

unsigned char gdb_resp_buffer[256];

unsigned char *gdb_resp_ptr;

static int make_socket_nonblocking (int sock)
{
	int res = fcntl (sock, F_SETFL, O_NONBLOCK);
	if (res < 0)
		gdb_printf ("Warning: could not make socket non-blocking\n");
	return (res);
}

static void
gdb_write (unsigned char c)
{
	*gdb_resp_ptr++ = c;
}

void
gdb_write2x (unsigned char val)
{
#define hexchar(v) (((v) >= 10) ? ((v) - 10 + 'A') : ((v) + '0'))
	gdb_write (hexchar (val >> 4));
	gdb_write (hexchar (val & 0xF));
}


static void
gdb_init_response (void)
{
	gdb_resp_ptr = gdb_resp_buffer;
	gdb_write ('$');
}


static void
gdb_finish_response (void)
{
	unsigned char *p;
	unsigned char check = 0;

	p = gdb_resp_buffer + 1;
	while (p++ < gdb_resp_ptr)
		check += *p;

	gdb_write ('#');
	gdb_write2x (check);
	*gdb_resp_ptr = '\0';
	printf ("[Resp: %s]\n", gdb_resp_buffer);
	write (gdb_client_socket, gdb_resp_buffer, gdb_resp_ptr - gdb_resp_buffer);
}


void
gdb_reply_ok (void)
{
	gdb_write ('O');
	gdb_write ('K');
}


void
gdb_reply_error (unsigned char errval)
{
	gdb_write ('E');
	gdb_write (' ');
	gdb_write2x (errval);
}

void
gdb_process_general_query (void)
{
	if (!strcmp (gdb_cmd_ptr, "C"))
	{
		gdb_write ('q');
		gdb_write ('C');
		gdb_write2x (program_pid);
	}
}


void
gdb_process_command (void)
{
	printf ("[Cmd: %s]\n", gdb_cmd_ptr);
	gdb_init_response ();
	switch (*gdb_cmd_ptr++)
	{
		case 'q':
			gdb_process_general_query ();
			break;

		case 'H':
			gdb_reply_ok ();
			break;

		case 'g':
		case 'G':
		case 'm':
		case 'c':
		case 's':
			break;
	}
	gdb_finish_response ();
}


void
gdb_reset_command (void)
{
	gdb_cmd_ptr = gdb_cmd_buffer;
}

void
gdb_ack_command (void)
{
	unsigned char b = '+';
	printf ("[ACK cmd]\n");
	write (gdb_client_socket, &b, 1);
}

void
gdb_nak_command (void)
{
	unsigned char b = '-';
	printf ("[NAK cmd]\n");
	write (gdb_client_socket, &b, 1);
}

/** Called by the simulator periodically to see if there is anything
 * to do with regards to GDB. */
void
gdb_periodic_task (void)
{
	switch (gdb_state)
	{
		case GDB_NOT_INITIALIZED:
			break;

		case GDB_INITIALIZED:
		{
			int addrlen;

			gdb_client_socket = accept (gdb_socket, 
				(struct sockaddr *)&gdb_addr, &addrlen);
			if (gdb_client_socket > 0)
			{
				gdb_printf ("Connection from GDB established.\n");
				gdb_state = GDB_CONNECTED;

				make_socket_nonblocking (gdb_client_socket);
				gdb_reset_command ();
			}
			break;
		}

		case GDB_CONNECTED:
		{
			int res = read (gdb_client_socket, gdb_cmd_ptr, 1);
			if (read == 0)
			{
				gdb_printf ("Connecting from GDB lost.\n");
				close (gdb_client_socket);
				gdb_state = GDB_INITIALIZED;
			}
			else if (res > 0)
			{
				if ((*gdb_cmd_ptr == '\r')
					|| (*gdb_cmd_ptr == '\n')
					|| (*gdb_cmd_ptr == ' '))
					;
				else if (*gdb_cmd_ptr == '+')
				{
					gdb_printf ("GDB: ACK resp\n");
				}
				else if (*gdb_cmd_ptr == '-')
				{
					gdb_printf ("GDB: NAK resp\n");
				}
				else if (*gdb_cmd_ptr == '$')
				{
				}
				else if (*gdb_cmd_ptr == '#')
				{
					unsigned char check[2];
					read (gdb_client_socket, check, 2);

					gdb_ack_command ();

					*gdb_cmd_ptr = '\0';
					gdb_reset_command ();

					gdb_process_command ();

					gdb_reset_command ();
				}
				else
					gdb_cmd_ptr++;
			}
			break;
		}
	}
}


int
gdb_init (void)
{
	int opt;
	struct sockaddr_in addr;
	int res;

	gdb_printf ("Initializing GDB stub\n");
	
	gdb_state = GDB_NOT_INITIALIZED;

	gdb_socket = socket (PF_INET, SOCK_STREAM, 0);
	if (gdb_socket < 0)
	{
		gdb_printf ("Couldn't create server socket\n");
	}

	opt = 1;
	res = setsockopt (gdb_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
	if (res < 0)
	{
		gdb_printf ("Couldn't create server socket\n");
		goto close_socket;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons (6809);
	addr.sin_addr.s_addr = INADDR_ANY;
	res = bind (gdb_socket, (struct sockaddr *)&addr, sizeof (addr));
	if (res < 0)
	{
		gdb_printf ("Couldn't bind to local address\n");
		goto close_socket;
	}

	res = listen (gdb_socket, 1);
	if (res < 0)
	{
		gdb_printf ("Couldn't listen on socket\n");
		goto close_socket;
	}

	make_socket_nonblocking (gdb_socket);

	gdb_printf ("OK... waiting for GDB to connect\n");
	gdb_state = GDB_INITIALIZED;
	return 0;

close_socket:
	close (gdb_socket);

	return -1;
}

