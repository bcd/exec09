
void sys_console_write (char c)
{
	*((unsigned char *)0xE000) = c;
}


char sys_console_read (void)
{
	return *((unsigned char *)0xE002);
}


