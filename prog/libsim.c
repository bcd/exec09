

void sys_console_write (char c)
{
	*((unsigned char *)0xE000) = c;
}


char sys_console_read (void)
{
	return *((unsigned char *)0xE002);
}


int sys_read (int fd, const char *buf, int len)
{
	return -1;
}


int sys_write (int fd, const char *buf, int len)
{
	switch (fd)
	{
		case 1:
		case 2:
			while (len-- > 0)
				sys_console_write (*buf++);
			return 0;

		default:
			return -1;
	}
}


