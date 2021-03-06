/* Zero Merge - combine files with empty (zero) blocks
 * Copyright (C) 2020 by Jody Bruchon <jody@jodybruchon.com>
 * Distributed under The MIT License
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include "version.h"

/* Block size to scan for merging */
#define BSIZE 4096
/* File read size */
#define READSIZE 1048576

#ifdef ON_WINDOWS
 #define FOPEN_R "rbS"
 #define FOPEN_W "wbS"
#else
 #define FOPEN_R "rb"
 #define FOPEN_W "wb"
#endif /* ON_WINDOWS */

static char program_name[PATH_MAX + 4];
static FILE *file1, *file2, *file3;

void clean_exit(void)
{
	if (file1) fclose(file1);
	if (file2) fclose(file2);
	if (file3) fclose(file3);
	return;
}

static void version(void)
{
	printf("zeromerge %s (%s) by Jody Bruchon <jody@jodybruchon.com>\n", VER, VERDATE);
	printf("Latest versions and support: https://github.com/jbruchon/zeromerge\n");
	return;
}


static void usage(void)
{
	printf("Usage: %s file1 file2 outfile\n", program_name);
	return;
}


int main(int argc, char **argv)
{
	static char buf1[READSIZE];
	static char buf2[READSIZE];
	struct stat stat1, stat2;
	off_t remain;
	off_t read1, read2, write;

	atexit(clean_exit);
	strncpy(program_name, argv[0], PATH_MAX);

	/* Help text if requested */
	if (argc >= 2) {
	       if ((!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
			version();
			usage();
			exit(EXIT_SUCCESS);
		}
	       if ((!strcmp(argv[1], "-v") || !strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))) {
			version();
			exit(EXIT_SUCCESS);
		}
	}

	if (argc != 4) goto error_argcount;

	/* Open files to merge */
	file1 = fopen(argv[1], FOPEN_R);
	if (!file1) goto error_file1;
	file2 = fopen(argv[2], FOPEN_R);
	if (!file2) goto error_file2;

	/* File sizes must match; sizes also needed for loop */
	if (stat(argv[1], &stat1) != 0) goto error_file1;
	if (stat(argv[2], &stat2) != 0) goto error_file2;
	if (stat1.st_size != stat2.st_size) goto error_file_sizes;
	remain = stat1.st_size;

	/* If read and size check are OK, open file to write into */
	file3 = fopen(argv[3], FOPEN_W);
	if (!file3) goto error_file3;

	/* Main loop */
	while (remain > 0) {

		read1 = (off_t)fread(&buf1, 1, READSIZE, file1);
		read2 = (off_t)fread(&buf2, 1, READSIZE, file2);
		if ((read1 != read2)) goto error_short_read;
		if (ferror(file1)) goto error_file1;
		if (ferror(file2)) goto error_file2;

		/* Merge data from last block to first block */
		while (read1--) {
			/* if blocks are non-zero and do not match, exit with error*/
			if (buf1[read1] != buf2[read1] && buf1[read1] != 0 && buf2[read1] != 0)
				goto error_different;
			/* merge data into buf1*/
			buf1[read1] |= buf2[read1];
		}
		write = (off_t)fwrite(&buf1, 1, (size_t)read2, file3);
		if (write != read2) goto error_short_write;
		remain -= read2;
		if (feof(file1) && feof(file2)) break;
	}
	exit(EXIT_SUCCESS);

error_different:
	fprintf(stderr, "Error: files contain different non-zero data\n");
	exit(EXIT_FAILURE);
error_argcount:
	usage();
	exit(EXIT_FAILURE);
error_file1:
	fprintf(stderr, "Error opening/reading '%s'\n", argv[1]);
	exit(EXIT_FAILURE);
error_file2:
	fprintf(stderr, "Error opening/reading '%s'\n", argv[2]);
	exit(EXIT_FAILURE);
error_file3:
	fprintf(stderr, "Error opening/writing '%s'\n", argv[3]);
	exit(EXIT_FAILURE);
error_file_sizes:
	fprintf(stderr, "Error: file sizes are not identical\n");
	exit(EXIT_FAILURE);
error_short_read:
	fprintf(stderr, "Error: short read\n");
	exit(EXIT_FAILURE);
error_short_write:
	fprintf(stderr, "Error: short write (%ld != %ld or %ld)\n", (long)write, (long)read1, (long)read2);
	exit(EXIT_FAILURE);
}
