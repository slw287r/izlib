#ifndef iZLIB_H
#define iZLIB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "igzip_lib.h"

#ifndef UNIX
#define UNIX 3
#endif

#ifndef BUF_SIZE
#define BUF_SIZE (1<<22)
#endif

#ifndef HDR_SIZE
#define HDR_SIZE (1<<16)
#endif

#ifndef MIN_COM_LVL
#define MIN_COM_LVL 0
#endif

#ifndef MAX_COM_LVL
#define MAX_COM_LVL 3
#endif

#ifndef COM_LVL_DEFAULT
#define COM_LVL_DEFAULT 3 // was 2
#endif

const int com_lvls[4] = {
	ISAL_DEF_LVL0_DEFAULT,
	ISAL_DEF_LVL1_DEFAULT,
	ISAL_DEF_LVL2_DEFAULT,
	ISAL_DEF_LVL3_DEFAULT
};

typedef struct
{
	FILE *fp;
	char *mode;
	int is_plain;
	struct isal_gzip_header *gzip_header;
	struct inflate_state *state;
	struct isal_zstream *zstream;
	uint8_t *buf_in;
	size_t buf_in_size;
	uint8_t *buf_out;
	size_t buf_out_size;
} gzFile_t;

typedef gzFile_t* gzFile;

#ifdef __cplusplus
extern "C" {
#endif
int is_gz(FILE* fp);
int is_plain(FILE* fp);
uint32_t get_posix_filetime(FILE* fp);
int ingest_gzip_header(gzFile fp);
gzFile gzopen(const char *in, const char *mode);
gzFile gzdopen(int fd, const char *mode);
int gzread(gzFile fp, void *buf, size_t len);
int gzwrite(gzFile fp, void *buf, size_t len);
int gzeof(gzFile fp);
int set_compress_level(gzFile fp, int level);
void gzclose(gzFile fp);
#ifdef __cplusplus
}
#endif

int is_gz(FILE* fp)
{
	if(!fp) return 0;
	char buf[2];
	int gzip = 0;
	if(fread(buf, 1, 2, fp) == 2)
		if(((int)buf[0] == 0x1f) && ((int)(buf[1]&0xFF) == 0x8b))
			gzip = 1; // normal gzip
	fseek(fp, 12, SEEK_SET);
	if(fread(buf, 1, 2, fp) == 2)
		if(gzip == 1 && (int)buf[0] == 0x42 && (int)(buf[1]&0xFF) == 0x43)
			gzip = 2; // bgzf format, need to require the normal gzip header
	fseek(fp, 0, SEEK_SET);
	return gzip;
}

int is_plain(FILE* fp)
{
	int r = 0;
	if(!feof(fp))
	{
		int c = getc(fp);
		r = isprint(c);
		ungetc(c, fp);
	}
	return r;
}

uint32_t get_posix_filetime(FILE* fp)
{
	struct stat file_stats;
	fstat(fileno(fp), &file_stats);
	return file_stats.st_mtime;
}

int ingest_gzip_header(gzFile fp) {
	// assume fp->state->avail_in > 0
	int status = isal_read_gzip_header(fp->state, fp->gzip_header);
	while (status == ISAL_END_INPUT && !feof(fp->fp)) {
		fp->state->next_in = fp->buf_in;
		fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
		status = isal_read_gzip_header(fp->state, fp->gzip_header);
	}
	return status;
}

gzFile gzopen(const char *in, const char *mode)
{
	gzFile fp = (gzFile_t *)calloc(1, sizeof(gzFile_t));
	fp->fp = fopen(in, mode);
	if(!fp->fp)
	{
		gzclose(fp);
		return NULL;
	}
	fp->mode = strdup(mode);
	// plain file
	if(*mode == 'r' && (fp->is_plain = !is_gz(fp->fp)))
		return fp;
	// gz file
	fp->gzip_header = (struct isal_gzip_header *)calloc(1, sizeof(struct isal_gzip_header));
	isal_gzip_header_init(fp->gzip_header);
	if (*mode == 'r') // read
	{
		fp->state = (struct inflate_state *)calloc(1, sizeof(struct inflate_state));
		fp->buf_in_size = BUF_SIZE;
		fp->buf_in = (uint8_t *)malloc(fp->buf_in_size * sizeof(uint8_t));
		isal_inflate_init(fp->state);
		fp->state->crc_flag = ISAL_GZIP_NO_HDR_VER;
		fp->state->next_in = fp->buf_in;
		fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
		if (ingest_gzip_header(fp) != ISAL_DECOMP_OK)
		{
			gzclose(fp);
			return NULL;
		}
	}
	else if (*mode == 'w') // write
	{
		fp->gzip_header->os = UNIX; // FIXME auto parse OS
		fp->gzip_header->time = get_posix_filetime(fp->fp);
		fp->gzip_header->name = strdup(in);
		fp->gzip_header->name_buf_len = strlen(fp->gzip_header->name) + 1;
		fp->buf_out_size = BUF_SIZE;
		fp->buf_out = (uint8_t *)calloc(fp->buf_out_size, sizeof(uint8_t));
		fp->zstream = (struct isal_zstream *)calloc(1, sizeof(struct isal_zstream));
		isal_deflate_init(fp->zstream);
		fp->zstream->avail_in = 0;
		fp->zstream->flush = NO_FLUSH;
		fp->zstream->level = COM_LVL_DEFAULT;
		fp->zstream->level_buf_size = com_lvls[fp->zstream->level];
		fp->zstream->level_buf = (uint8_t *)calloc(fp->zstream->level_buf_size, sizeof(uint8_t));
		fp->zstream->gzip_flag = IGZIP_GZIP_NO_HDR;
		fp->zstream->avail_out = fp->buf_out_size;
		fp->zstream->next_out = fp->buf_out;
		if(isal_write_gzip_header(fp->zstream, fp->gzip_header) != ISAL_DECOMP_OK)
		{
			gzclose(fp);
			return NULL;
		}
	}
	return fp;
}

gzFile gzdopen(int fd, const char *mode)
{
	char path[10];         /* identifier for error messages */
	if (fd == -1)
		return NULL;
	sprintf(path, "<fd:%d>", fd);   /* for debugging */
	gzFile fp = (gzFile_t *)calloc(1, sizeof(gzFile_t));
	if(!(fp->fp = fdopen(fd, mode)))
	{
		gzclose(fp);
		return NULL;
	}
	fp->mode = strdup(mode);
	// plain file
	if(*mode == 'r' && (fp->is_plain = is_plain(fp->fp)))
		return fp;
	// gz file
	fp->gzip_header = (struct isal_gzip_header *)calloc(1, sizeof(struct isal_gzip_header));
	isal_gzip_header_init(fp->gzip_header);
	if (*mode == 'r') // read
	{
		fp->state = (struct inflate_state *)calloc(1, sizeof(struct inflate_state));
		fp->buf_in_size = BUF_SIZE;
		fp->buf_in = (uint8_t *)malloc(fp->buf_in_size * sizeof(uint8_t));
		isal_inflate_init(fp->state);
		fp->state->crc_flag = ISAL_GZIP_NO_HDR_VER;
		fp->state->next_in = fp->buf_in;
		fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
		if (ingest_gzip_header(fp) != ISAL_DECOMP_OK)
		{
			gzclose(fp);
			return NULL;
		}
	}
	else if (*mode == 'w') // write
	{
		fp->gzip_header->os = UNIX; // FIXME auto parse OS
		fp->gzip_header->time = get_posix_filetime(fp->fp);
		fp->gzip_header->name = strdup(path);
		fp->gzip_header->name_buf_len = strlen(fp->gzip_header->name) + 1;
		fp->buf_out_size = BUF_SIZE;
		fp->buf_out = (uint8_t *)calloc(fp->buf_out_size, sizeof(uint8_t));
		fp->zstream = (struct isal_zstream *)calloc(1, sizeof(struct isal_zstream));
		isal_deflate_init(fp->zstream);
		fp->zstream->avail_in = 0;
		fp->zstream->flush = NO_FLUSH;
		fp->zstream->level = COM_LVL_DEFAULT;
		fp->zstream->level_buf_size = com_lvls[fp->zstream->level];
		fp->zstream->level_buf = (uint8_t *)calloc(fp->zstream->level_buf_size, sizeof(uint8_t));
		fp->zstream->gzip_flag = IGZIP_GZIP_NO_HDR;
		fp->zstream->avail_out = fp->buf_out_size;
		fp->zstream->next_out = fp->buf_out;
		if(isal_write_gzip_header(fp->zstream, fp->gzip_header) != ISAL_DECOMP_OK)
		{
			gzclose(fp);
			return NULL;
		}
	}
	return fp;
}

void gzclose(gzFile fp)
{
	if(!fp) return;
	if(fp->mode) free(fp->mode);
	if(fp->zstream && fp->fp) gzwrite(fp, NULL, 0);
	if(fp->gzip_header)
	{
		if(fp->gzip_header->extra) free(fp->gzip_header->extra);
		if(fp->gzip_header->name) free(fp->gzip_header->name);
		if(fp->gzip_header->comment) free(fp->gzip_header->comment);
		free(fp->gzip_header);
	}
	if(fp->state) free(fp->state);
	if(fp->buf_in) free(fp->buf_in);
	if(fp->buf_out) free(fp->buf_out);
	if(fp->zstream){
		if(fp->zstream->level_buf) free(fp->zstream->level_buf);
		free(fp->zstream);
	}
	if(fp->fp) fclose(fp->fp);
	free(fp);
}

int gzread(gzFile fp, void *buf, size_t len)
{
	int buf_data_len = 0;
	if (fp->is_plain)
	{
		if(!feof(fp->fp))
			buf_data_len = fread((uint8_t *)buf, 1, len, fp->fp);
		return buf_data_len;
	}
	do // Start reading in compressed data and decompress
	{
		if (!feof(fp->fp) && !fp->state->avail_in)
		{
			fp->state->next_in = fp->buf_in;
			fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
		}
		fp->state->next_out = (uint8_t *)buf;
		fp->state->avail_out = len;
		if (isal_inflate(fp->state) != ISAL_DECOMP_OK)
			return -3;
		if ((buf_data_len = fp->state->next_out - (uint8_t *)buf))
			return buf_data_len;
	} while (fp->state->block_state != ISAL_BLOCK_FINISH // while not done
		&& (!feof(fp->fp) || !fp->state->avail_out)); // and work to do
	// Add the following to look for and decode additional concatenated files
	if (!feof(fp->fp) && !fp->state->avail_in)
	{
		fp->state->next_in = fp->buf_in;
		fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
	}
	while (fp->state->avail_in && fp->state->next_in[0] == 31) // 0x1f
	{
		// Look for magic numbers for gzip header. Follows the gzread() decision
		// whether to treat as trailing junk
		if (fp->state->avail_in > 1 && fp->state->next_in[1] != 139) // 0x8b
			break;
		isal_inflate_reset(fp->state);
		isal_gzip_header_init(fp->gzip_header);
		fp->state->crc_flag = ISAL_GZIP_NO_HDR_VER;
		if (ingest_gzip_header(fp) != ISAL_DECOMP_OK) return -3; // fail to parse header
		do
		{
			if (!feof(fp->fp) && !fp->state->avail_in)
			{
				fp->state->next_in = fp->buf_in;
				fp->state->avail_in = fread(fp->state->next_in, 1, fp->buf_in_size, fp->fp);
			}
			fp->state->next_out = (uint8_t *)buf;
			fp->state->avail_out = len;
			if (isal_inflate(fp->state) != ISAL_DECOMP_OK)
				return -3;
			if ((buf_data_len = fp->state->next_out - (uint8_t *)buf))
				return buf_data_len;
		} while (fp->state->block_state != ISAL_BLOCK_FINISH
				&& (!feof(fp->fp) || !fp->state->avail_out));
	}
	return buf_data_len;
}

int set_compress_level(gzFile fp, int level)
{
	if (!fp || !fp->mode || *fp->mode != 'w') return -1;
	if (level < MIN_COM_LVL || level > MAX_COM_LVL) return -1;
	if (fp->zstream->level != level)
	{
		fp->zstream->level = level;
		fp->zstream->level_buf_size = com_lvls[fp->zstream->level];
		fp->zstream->level_buf = (uint8_t *)realloc(fp->zstream->level_buf,
				fp->zstream->level_buf_size * sizeof(uint8_t));
	}
	return 0;
}

int gzwrite(gzFile fp, void *buf, size_t _len)
{
	fp->zstream->next_in = (uint8_t *)buf;
	fp->zstream->avail_in = _len;
	fp->zstream->end_of_stream = !buf;
	size_t len = 0;
	do
	{
		if(!fp->zstream->next_out)
		{
			fp->zstream->next_out = fp->buf_out;
			fp->zstream->avail_out = fp->buf_out_size;
		}
		int ret = isal_deflate(fp->zstream);
		if (ret != ISAL_DECOMP_OK) return -3;
		len += fwrite(fp->buf_out, 1, fp->zstream->next_out - fp->buf_out, fp->fp);
		fp->zstream->next_out = NULL;
	} while (!fp->zstream->avail_out);
	return len;
}

int gzeof(gzFile fp)
{
    if(!fp) return 0;
    if(fp->mode[0] != 'w' || fp->mode[0] != 'r') return 0;
    return fp->mode[0] == 'r' ? feof(fp->fp) : 0;
}

#endif
