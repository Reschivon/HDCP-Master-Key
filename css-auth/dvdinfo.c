/*
 * A noddy program for getting and printing some info from the
 * DVD-ROM drive.
 */

#include <stdio.h>
#include <fcntl.h>
#if defined(__OpenBSD__)
# include <sys/dvdio.h>
#elif defined(__linux__)
# include <linux/cdrom.h>
#else
# error "Need the DVD ioctls"
#endif
#include <sys/ioctl.h>
#include <errno.h>

#define DVD	"/dev/cdrom"

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

int GetPhysical(int fd)
{
	dvd_struct d;
	int layer = 0, layers = 4;

	d.physical.type = DVD_STRUCT_PHYSICAL;
	while (layer < layers) {
		d.physical.layer_num = layer;
	
		if (ioctl(fd, DVD_READ_STRUCT, &d)<0)
		{
			printf("Could not read Physical layer %d\n", layer);
			return 0;
		}

		layers = d.physical.layer[layer].nlayers + 1;

		printf("Layer %d[%d]\n", layer, layers);
		printf(" Book Version:   %d\n", d.physical.layer[layer].book_version);
		printf(" Book Type:      %d\n", d.physical.layer[layer].book_type);
		printf(" Min Rate:       %d\n", d.physical.layer[layer].min_rate);
		printf(" Disk Size:      %d\n", d.physical.layer[layer].disc_size);
		printf(" Layer Type:     %d\n", d.physical.layer[layer].layer_type);
		printf(" Track Path:     %d\n", d.physical.layer[layer].track_path);
		printf(" Num Layers:     %d\n", d.physical.layer[layer].nlayers);
		printf(" Track Density:  %d\n", d.physical.layer[layer].track_density);
		printf(" Linear Density: %d\n", d.physical.layer[layer].linear_density);
		printf(" BCA:            %d\n", d.physical.layer[layer].bca);
		printf(" Start Sector    %#x\n", d.physical.layer[layer].start_sector);
		printf(" End Sector      %#x\n", d.physical.layer[layer].end_sector);
		printf(" End Sector L0   %#x\n", d.physical.layer[layer].end_sector_l0);

		++layer;
	}

	return 1;
}

int GetCopyright(int fd)
{
	dvd_struct d;

	d.copyright.type = DVD_STRUCT_COPYRIGHT;
	d.copyright.layer_num = 0;
	
	if (ioctl(fd, DVD_READ_STRUCT, &d)<0)
	{
		printf("Could not read Copyright Struct\n");
		return 0;
	}

	printf("Copyright: CPST=%d, RMI=%#02x\n", d.copyright.cpst, d.copyright.rmi);

	return 1;
}

int main(int ac, char **av)
{
	int fd;
	char *device = DVD;

	if (ac > 1)
		device = av[1];

	fd = open(device, O_RDONLY | O_NONBLOCK);

	if (fd < 0) {
		printf("unable to open dvd drive (%s).\n", device);
		return 1;
	}

	GetASF(fd);

	GetPhysical(fd);
	GetCopyright(fd);

	return 0;
}
