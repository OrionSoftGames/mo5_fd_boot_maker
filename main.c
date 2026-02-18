#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

unsigned char	*d7buffer;
int	d7pos = 0;
int	Ntracks;

void	ComputeBootCheckSum(unsigned char *buf)
{
	unsigned char	sum = 0x55, size = 127;

	// Only the first 127 bytes are crypted/checksumed !
	while (size--)
	{
		unsigned char	c = *buf;

		sum += c;		// Compute Checksum
		c = (~c) + 1;	// Encrypt Data
		*buf++ = c;		// Store new data
	}
	*buf = sum;	// Checksum at the end
}

// Return Position on D7 in spec long format
uint32_t	AddFileToD7buffer(char *filename, int *fsize)
{
	FILE		*in;
	int			size, ns, t, sect;
	uint32_t	info;

	in = fopen(filename, "rb");
	if (!in)
	{
		printf("File '%s' not found !\n", filename);
		system("PAUSE");
		exit(0);
	}
	fseek(in, 0, SEEK_END);
	size = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (fsize)
		*fsize = size;
	fread(&d7buffer[d7pos], 1, size, in);
	fclose(in);
	ns = size / 256;	// nsect
	if ((ns * 256) != size)
		ns++;			// round up to 256
	t = d7pos / (16 * 256);	// track
	sect = 1+((d7pos >> 8) & 15);

	// Start Track / Start Sector / N Sectors
	info = (t << 16) | (sect << 8) | ns;
	printf("%16s Track:%2d Sect:%02d nSect:%2d\n", filename, t, sect, ns);
	d7pos += (ns * 256);
	return (info);
}


int		main(int argc, char **argv)
{
	int			i, size;
	FILE		*out, *ifo;
	char		fl[32];
	uint32_t	nfo;

	printf("MO5 FD Disk Generator - by Orion_ [2019-2024]\n\n");

	if (argc < 5)
	{
		printf("Usage:\tfd_boot_maker FDfile InfoIncFile Ntracks bootsector.bin [data files ...]\n\ninfoincfile = *.h for C header output. (otherwise default to ASM output)\n\n");
		system("pause");
		return (1);
	}

	// Init
	Ntracks = atoi(argv[3]);
	if (Ntracks > 80)
	{
		printf("Too much tracks ! %d > 80\n", Ntracks);
		return (1);
	}
	d7buffer = malloc(16*256*Ntracks);
	memset(d7buffer, 0, 16*256*Ntracks);

	// Include BootSector
	out = fopen(argv[4], "rb");
	if (!out)
	{
		printf("Cannot open BootSector file '%s'\n", argv[4]);
		return (1);
	}
	fseek(out, 0, SEEK_END);
	i = ftell(out);
	if (i > 255)
	{
		fclose(out);
		printf("BootSector file too large ! %d > 255\n", i);
		return (1);
	}
	fseek(out, 0, SEEK_SET);
	fread(d7buffer, 1, i, out);
	fclose(out);
	ComputeBootCheckSum(d7buffer);
	d7pos = 256;

	// Generate Include Info File about Files Position on D7
	ifo = fopen(argv[2], "wb");
	if (ifo)
	{
		if (strstr(argv[2], ".h"))
			fprintf(ifo, "// FD Disk Generator - by Orion_ [2019-2024]\r\n\r\n");
		else
			fprintf(ifo, "; FD Disk Generator - by Orion_ [2019-2024]\r\n\r\n");

		// Include Files
		for (i = 5; i < argc; i++)
		{
			nfo = AddFileToD7buffer(argv[i], &size);
			strcpy(fl, "F_");
			strcat(fl, argv[i]);
			*strchr(fl, '.') = '_';
			if (strstr(argv[2], ".h"))
			{
				fprintf(ifo, "#define	%s\t0x%04x\r\n", fl, nfo>>8);
				fl[0] = 'S';
				fprintf(ifo, "#define	%s\t0x%02x\r\n", fl, nfo&0xFF);
				fl[0] = 'L';
				fprintf(ifo, "#define	%s\t%d\r\n", fl, size);
			}
			else
			{
				fprintf(ifo, "%s\tequ\t$%04x\r\n", fl, nfo>>8);
				fl[0] = 'S';
				fprintf(ifo, "%s\tequ\t$%02x\r\n", fl, nfo&0xFF);
				fl[0] = 'L';
				fprintf(ifo, "%s\tequ\t%d\r\n", fl, size);
			}
		}

		fclose(ifo);

		// Write Out the D7 Image File
		out = fopen(argv[1], "wb");
		if (out)
		{
			fwrite(d7buffer, 1, 16*256*Ntracks, out);
			fclose(out);
		}
	}
	free(d7buffer);

	return (0);
}
