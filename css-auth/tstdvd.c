/*
 * tstdvd.c
 *
 * Example program showing usage of DVD CSS ioctls
 *
 * Copyright (C) 1999 Andrew T. Veliath <andrewtv@usa.net>
 * See http://www.rpi.edu/~veliaa/linux-dvd for more info.
 */

/* Hacked about by Derek Fawcus <derek@spider.com> such that
 * it can be used as a simple program to authenticate the
 * computer with the DVD-ROM drive.
 *
 * If supplied with one parameter it gets the disk key and
 * saves it to a file.  If supplied with a second parameter
 * (a LBA) then it gets the title key for the supplied LBA.
 *
 * When getting the disk key,  only the first 10 bytes of it
 * are printed.  The whole key is written to the file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#if defined(__OpenBSD__)
# include <sys/dvdio.h>
#elif defined(__linux__)
# include <linux/cdrom.h>
#else
# error "Need the DVD ioctls"
#endif
#include "css-auth.h"

byte Challenge[10];
struct block Key1;
struct block Key2;
struct block KeyCheck;
byte DiscKey[10];
int varient = -1;

void print_challenge(const byte *chal)
{
	int i;

	for (i = 0; i < 10; ++i)
		printf(" %02X", chal[9-i] & 0xff);
}

void print_key(const byte *key)
{
	int i;

	for (i = 0; i < 5; ++i)
		printf(" %02X", key[4-i] & 0xff);
}


void print_five(const byte *key)
{
	int i;

	for (i = 0; i < 5; ++i)
		printf(" %02X", key[i] & 0xff);
}

int authenticate_drive(const byte *key)
{
	int i;

	for (i=0; i<5; i++)
		Key1.b[i] = key[4-i];

	for (i = 0; i < 32; ++i)
	{
		CryptKey1(i, Challenge, &KeyCheck);
		if (memcmp(KeyCheck.b, Key1.b, 5)==0)
		{
			varient = i;
			printf("Drive Authentic - using varient %d\n", i);
			return 1;
		}
	}

	if (varient == -1)
		printf("Drive would not Authenticate\n");

	return 0;
}

int GetDiscKey(int fd, int agid, char *key)
{
	dvd_struct s;
	int	index, fdd;

	s.type = DVD_STRUCT_DISCKEY;
	s.disckey.agid = agid;
	memset(s.disckey.value, 0, 2048);
	if (ioctl(fd, DVD_READ_STRUCT, &s)<0)
	{
		printf("Could not read Disc Key\n");
		return 0;
	}

	printf ("Received Disc Key:\t");
	for (index=0; index<sizeof s.disckey.value; index++)
		s.disckey.value[index] ^= key[4 - (index%5)];
	for (index=0; index<10; index++) {
		printf("%02X ", s.disckey.value[index]);
	}		
	printf ("\n");

	fdd = open("disk-key", O_WRONLY|O_TRUNC|O_CREAT, 0644);
	if (fdd < 0)
		printf("Can't create \"disk-key\"\n");
	else {
		if (write(fdd, s.disckey.value, 2048) != 2048)
			printf("Can't write \"disk-key\"\n");
		close(fdd);
	}

	return 1;
}

int GetTitleKey(int fd, int agid, int lba, char *key)
{
	dvd_authinfo ai;
	int i, fdd;

	ai.type = DVD_LU_SEND_TITLE_KEY;

	ai.lstk.agid = agid;
	ai.lstk.lba = lba;

	if (ioctl(fd, DVD_AUTH, &ai)) {
		printf("GetTitleKey failed\n");
		return 0;
	}

	printf ("Received Title Key:\t");
	for (i = 0; i < 5; ++i) {
		ai.lstk.title_key[i] ^= key[4 - (i%5)];
		printf("%02X ", ai.lstk.title_key[i]);
	}
	putchar('\n');

	printf(" CPM=%d, CP_SEC=%d, CGMS=%d\n", ai.lstk.cpm, ai.lstk.cp_sec, ai.lstk.cgms);

	fdd = open("title-key", O_WRONLY|O_TRUNC|O_CREAT, 0644);
	if (fdd < 0)
		printf("Can't create \"title-key\"\n");
	else {
		if (write(fdd, ai.lstk.title_key, 5) != 5)
			printf("Can't write \"title-key\"\n");
		close(fdd);
	}

	return 1;
}

int GetASF(int fd)
{
	dvd_authinfo ai;

	ai.type = DVD_LU_SEND_ASF;
	ai.lsasf.agid = 0;
	ai.lsasf.asf = 0;

	if (ioctl(fd, DVD_AUTH, &ai)) {
		printf("GetASF failed\n");
		return 0;
	}

	printf("%sAuthenticated\n", (ai.lsasf.asf) ? "" : "not ");

	return 1;
}

/* Simulation of a non-CSS compliant host (i.e. the authentication fails,
 * but idea is here for a real CSS compliant authentication scheme). */
int
hostauth (dvd_authinfo *ai)
{
	int i;

	switch (ai->type) {
	/* Host data receive (host changes state) */
	case DVD_LU_SEND_AGID:
		printf("AGID %d\n", ai->lsa.agid);
		ai->type = DVD_HOST_SEND_CHALLENGE;
		break;

	case DVD_LU_SEND_KEY1:
		printf("LU sent key1: "); print_key(ai->lsk.key); printf("\n");
		if (!authenticate_drive(ai->lsk.key)) {
			ai->type = DVD_AUTH_FAILURE;
			return -EINVAL;
		}
		ai->type = DVD_LU_SEND_CHALLENGE;
		break;

	case DVD_LU_SEND_CHALLENGE:
		for (i = 0; i < 10; ++i)
			Challenge[i] = ai->hsc.chal[9-i];
		printf("LU sent challenge: "); print_challenge(Challenge); printf("\n");
		CryptKey2(varient, Challenge, &Key2);
		ai->type = DVD_HOST_SEND_KEY2;
		break;

	/* Host data send */
	case DVD_HOST_SEND_CHALLENGE:
		for (i = 0; i < 10; ++i)
			ai->hsc.chal[9-i] = Challenge[i];
		printf("Host sending challenge: "); print_challenge(Challenge); printf("\n");
		/* Returning data, let LU change state */
		break;

	case DVD_HOST_SEND_KEY2:
		for (i = 0; i < 5; ++i)
			ai->hsk.key[4-i] = Key2.b[i];
		printf("Host sending key 2: "); print_key(Key2.b); printf("\n");
		/* Returning data, let LU change state */
		break;

	default:
		printf("Got invalid state %d\n", ai->type);
		return -EINVAL;
	}

	return 0;
}

int authenticate(int fd, int title, int lba)
{
	dvd_authinfo ai;
	dvd_struct dvds;
	int i, rv, tries, agid;

	memset(&ai, 0, sizeof (ai));
	memset(&dvds, 0, sizeof (dvds));

	GetASF(fd);

	/* Init sequence, request AGID */
	for (tries = 1, rv = -1; rv == -1 && tries < 4; ++tries) {
		printf("Request AGID [%d]...\t", tries);
		ai.type = DVD_LU_SEND_AGID;
		ai.lsa.agid = 0;
		rv = ioctl(fd, DVD_AUTH, &ai);
		if (rv == -1) {
			perror("N/A, invalidating");
			ai.type = DVD_INVALIDATE_AGID;
			ai.lsa.agid = 0;
			ioctl(fd, DVD_AUTH, &ai);
		}
	}
	if (tries == 4) {
		printf("Cannot get AGID\n");
		return -1;
	}

	for (i = 0; i < 10; ++i)
		Challenge[i] = i;

	/* Send AGID to host */
	if (hostauth(&ai) < 0) {
		printf("Send AGID to host failed\n");
		return -1;
	}
	/* Get challenge from host */
	if (hostauth(&ai) < 0) {
		printf("Get challenge from host failed\n");
		return -1;
	}
	agid = ai.lsa.agid;
	/* Send challenge to LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		printf("Send challenge to LU failed\n");
		return -1;
	}
	/* Get key1 from LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		printf("Get key1 from LU failed\n");
		return -1;
	}
	/* Send key1 to host */
	if (hostauth(&ai) < 0) {
		printf("Send key1 to host failed\n");
		return -1;
	}
	/* Get challenge from LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		printf("Get challenge from LU failed\n");
		return -1;
	}
	/* Send challenge to host */
	if (hostauth(&ai) < 0) {
		printf("Send challenge to host failed\n");
		return -1;
	}
	/* Get key2 from host */
	if (hostauth(&ai) < 0) {
		printf("Get key2 from host failed\n");
		return -1;
	}
	/* Send key2 to LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		printf("Send key2 to LU failed (expected)\n");
		return -1;
	}

	if (ai.type == DVD_AUTH_ESTABLISHED)
		printf("DVD is authenticated\n");
	else if (ai.type == DVD_AUTH_FAILURE)
		printf("DVD authentication failed\n");

	memcpy(Challenge, Key1.b, 5);
	memcpy(Challenge+5, Key2.b, 5);
	CryptBusKey(varient, Challenge, &KeyCheck);
	printf("Received Session Key:\t");
	for (i= 0; i< 5; i++)
	{
		printf("%02X ", KeyCheck.b[i]);
	}
	printf("\n");

	GetASF(fd);

	if (title)
		GetTitleKey(fd, agid, lba, KeyCheck.b);
	else
		GetDiscKey(fd, agid, KeyCheck.b);

	GetASF(fd);

	return 0;
}

#ifndef FIBMAP
#define FIBMAP 1
#endif

int path_to_lba(char *p)
{
	int fd, lba = 0;

	if ((fd = open(p, O_RDONLY)) == -1) {
		perror("DVD vob file:");
		return 0;
	}
	if (ioctl(fd, FIBMAP, &lba) != 0) {
		perror("ioctl FIBMAP failed:");
		close(fd);
		return 0;
	}

	close(fd);

	return lba;
}

int main(int ac, char **av)
{
	char *device;
	int fd, title = 0, lba = 0;

	if (ac < 2) {
		fprintf(stderr, "usage: tstdvd <device> [title_path]\n");
		exit (1);
	}
	device = av[1];
	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror(device);
		exit(1);
	}
	if (ac == 3) {
		lba = path_to_lba(av[2]);
		title = 1;
	}
	authenticate(fd, title, lba);
	close(fd);

	return 0;
}
