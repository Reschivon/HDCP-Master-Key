/*
 * A noddy program which tries to reset all AGID's on the DVD-ROM drive.
 */

#include<stdio.h>
#include<fcntl.h>
#if defined(__OpenBSD__)
# include <sys/dvdio.h>
#elif defined(__linux__)
# include <linux/cdrom.h>
#else
# error "Need the DVD ioctls"
#endif
#include<sys/ioctl.h>
#include<errno.h>

static int fd;

#define DVD	"/dev/cdrom"

int main(int ac, char **av)
{
	dvd_authinfo ai;
	char *device = DVD;
	int i;

	if (ac > 1)
		device = av[1];

	fd = open(device, O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		printf("unable to open dvd drive (%s).\n", device);
		return 1;
	}

	for (i = 0; i < 4; i++) {
		memset(&ai, 0, sizeof(ai));
		ai.type = DVD_INVALIDATE_AGID;
		ai.lsa.agid = i;
		ioctl(fd, DVD_AUTH, &ai);
	}

	return 0;
}
