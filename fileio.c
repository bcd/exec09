
#include "6809.h"

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

	if (!path || strchr (filename, dirsep) || *mode == 'w')
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


FILE *
file_require_open (struct pathlist *path, const char *filename, const char *mode)
{
	FILE *fp = file_open (path, filename, mode);
	if (!fp)
		fprintf (stderr, "error: could not open '%s'\n", filename);
	return fp;
}


void
file_close (FILE *fp)
{
	fclose (fp);
}

