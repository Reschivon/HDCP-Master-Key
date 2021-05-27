/*
 * css-cat.c
 *
 * Copyright 1999 Derek Fawcus.
 *
 * Released under version 2 of the GPL.
 *
 * Decode selected sector types from a CSS encoded DVD to stdout.  Use as a
 * filter on the input to mpeg2player or ac3dec.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__)
# include <getopt.h>
#endif /* __linux__ */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "css-descramble.h"

static struct playkey pkey1a1 = {0x36b, {0x51,0x67,0x67,0xc5,0xe0}};
static struct playkey pkey2a1 = {0x762, {0x2c,0xb2,0xc1,0x09,0xee}};
static struct playkey pkey1b1 = {0x36b, {0x90,0xc1,0xd7,0x84,0x48}};

static struct playkey pkey1a2 = {0x2f3, {0x51,0x67,0x67,0xc5,0xe0}};
static struct playkey pkey2a2 = {0x730, {0x2c,0xb2,0xc1,0x09,0xee}};
static struct playkey pkey1b2 = {0x2f3, {0x90,0xc1,0xd7,0x84,0x48}};

static struct playkey pkey1a3 = {0x235, {0x51,0x67,0x67,0xc5,0xe0}};
static struct playkey pkey1b3 = {0x235, {0x90,0xc1,0xd7,0x84,0x48}};

static struct playkey pkey3a1 = {0x249, {0xb7,0x3f,0xd4,0xaa,0x14}}; /* DVD specific ? */
static struct playkey pkey4a1 = {0x028, {0x53,0xd4,0xf7,0xd9,0x8f}}; /* DVD specific ? */


static struct playkey *playkeys[] = {
	&pkey1a1, &pkey2a1, &pkey1b1,
	&pkey1a2, &pkey2a2, &pkey1b2,
	&pkey1a3, &pkey1b3,
	&pkey3a1, &pkey4a1,
	NULL};

static unsigned char disk_key[2048];
static unsigned char title_key[5];

static unsigned char sector[2048];

unsigned long sectors = 0;
unsigned long crypted = 0;
unsigned long skipped = 0;

int do_all = 0;
int do_video = 0;
int do_ac3 = 0;
int do_mpg = 0;
int verbose = 0;
int keep_pack = 0;
int keep_pes = -1;

#define STCODE(p,a,b,c,d) ((p)[0] == a && (p)[1] == b && (p)[2] == c && (p)[3] == d)

static void un_css(int fdi, int fdo)
{
	unsigned char *sp, *pes;
	int writen, wr, peslen, hdrlen;

	while (read(fdi, sector, 2048) == 2048) {
		++sectors;
		if (!STCODE(sector,0x00,0x00,0x01,0xba)) {
			fputs("Not Pack start code\n", stderr);
			++skipped; continue;
		}

		if (do_all)
			goto write_it;

		pes = sector + 14 + (sector[13] & 0x07);
		if (STCODE(pes,0x00,0x00,0x01,0xbb)) {/* System Header Pack Layer */
			peslen = (pes[0x04] << 8) + pes[0x05];
			pes += peslen + 6;
		}

		if (pes[0x00] || pes[0x01] || pes[0x02] != 0x01 || pes[0x03] < 0xbc) {
			++skipped; continue;
		}
		peslen = (pes[0x04] << 8) + pes[0x05];
		hdrlen = pes[0x08] + 6 + 3;
		if ((pes[0x03] & 0xf0) == 0xe0) {
			if (do_video)
				goto write_it;
		} else if (do_mpg && pes[0x03] == (0xc0 | (do_mpg - 1))) { /* MPEG Audio */
			goto write_it;
		} else if (pes[0x03] == 0xbd) { /* AC3 Audio */
			if (do_ac3) {
				int audiotrack = do_ac3 - 1;
				if (pes[hdrlen] == (0x80|(audiotrack & 7))) {
					hdrlen += 4;
					goto write_it;
				}
			}
		} else
			++skipped;
		continue;

	write_it:
		if (sector[20] & 0x30) {
			++crypted;
			css_descramble(sector, title_key);
			sector[20] &= 0x8f;
		}
		writen = 0;
		if (keep_pack)
			sp = sector, peslen = 2048;
		else if (keep_pes)
			sp = pes, peslen = 2048 - (pes - sector);
		else
			sp = pes + hdrlen, peslen -= hdrlen - 6;

		do {
			wr = write(fdo, sp, peslen - writen);
			sp += wr;
			writen += wr;
		} while (wr > 0 && writen < peslen);
	}
}

static void usage_exit(void)
{
	fputs("usage: css-cat [-t title-no] [-m mpeg-audio-no ] [-avPp12345678] vob_file\n", stderr);
	exit(2);
}

static char *title = "1";

static int parse_args(int ac, char **av)
{
	int c;
	opterr = 0;
	while (1)
		switch((c = getopt(ac, av, "at:Ppvm:01234567"))) {
		case 'a':
			do_all = 1;
			/* fall through */
		case 'P':
			keep_pack = 1;
			break;
		case 'p':
			keep_pes = 1;
			break;
		case 't':
			title = optarg;
			break;
		case 'v':
			do_video = 1;
			++keep_pes;
			break;
		case 'm':
			if ((do_mpg = atoi(optarg)) < 1 || do_mpg > 32)
				usage_exit();
			++keep_pes;
			break;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8':
			do_ac3 = c - '0';
			++keep_pes;
			break;
		case EOF:
			goto got_args;
		default:
			usage_exit();
			break;
		}

got_args:
	keep_pes = (keep_pes > 0) ? 1 : 0;

	return optind;
}

int main(int ac, char **av)
{
	int ai, fd;
	char titlef[12];

	if ((fd = open("disk-key", O_RDONLY)) == -1) {
		perror("can't open disk-key");
		exit(1);
	}
	if (read(fd, disk_key, 2048) != 2048) {
		perror("can't read disk-key");
		close(fd);
		exit(1);
	}
	close(fd);

	if ((ai = parse_args(ac, av)) >= ac)
		usage_exit();

	strcpy(titlef, "title");
	strcat(titlef, title);
	strcat(titlef, "-key");

	if ((fd = open(titlef, O_RDONLY)) == -1) {
		perror("can't open title-key");
		exit(1);
	}
	if (read(fd, title_key, 5) != 5) {
		perror("can't read title-key");
		close(fd);
		exit(1);
	}
	close(fd);

	if (strcmp(av[ai], "-") == 0)
		fd = 0;
	else if ((fd = open(av[ai], O_RDONLY)) == -1) {
		fputs("can't open VOB file ", stderr);
		fputs(av[ai], stderr);
		perror("");
		exit(1);
	}

	if (!css_decrypttitlekey(title_key, disk_key, playkeys)) {
		close(fd);
		return 3;
	}

	un_css(fd, 1);

	fprintf(stderr, "Total %lu, skipped %lu,  crvid %lu\n",
		sectors, skipped, crypted);

	close(fd);

	return 0;
}
