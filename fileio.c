
#include <stdio.h>

struct pathlist
{
	int count;
	char *entry[32];
};


void
path_init (struct pathlist *path)
{
	path->count = 0;
}

void
path_add (struct pathlist *path, const char *dir)
{
	char *dir2;
	dir2 = path->entry[path->count++] = malloc (strlen (dir) + 1);
	strcpy (dir2, dir);
}


FILE *
file_open (struct pathlist *path, const char *filename, const char *mode)
{
	FILE *fp;
	char fqname[128];
	int count;
	const char dirsep = '/';

	fp = fopen (filename, mode);
	if (fp)
		return fp;

	if (!path || strchr (filename, dirsep))
		return NULL;

	for (count = 0; count < path->count; count++)
	{
		sprintf (fqname, "%s%c%s", path->entry[count], dirsep, filename);
		fp = fopen (fqname, mode);
		if (fp)
			return fp;
	}

	return NULL;
}


void
file_close (FILE *fp)
{
	fclose (fp);
}

