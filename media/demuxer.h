#ifndef __DEMUXER_H
#define __DEMUXER_H 1

#ifdef GP2X
#include <pthread.h>
#endif
#include <stdlib.h>

#define MAX_PACKS 4096
#define MAX_PACK_BYTES 0x800000

#define DEMUXER_TYPE_UNKNOWN 0
#define DEMUXER_TYPE_MPEG_ES 1
#define DEMUXER_TYPE_MPEG_PS 2
#define DEMUXER_TYPE_AVI 3
#define DEMUXER_TYPE_AVI_NI 4
#define DEMUXER_TYPE_AVI_NINI 5
#define DEMUXER_TYPE_ASF 6
#define DEMUXER_TYPE_MOV 7
#define DEMUXER_TYPE_VIVO 8
#define DEMUXER_TYPE_TV 9
#define DEMUXER_TYPE_FLI 10
#define DEMUXER_TYPE_REAL 11
#define DEMUXER_TYPE_Y4M 12
#define DEMUXER_TYPE_NUV 13
#define DEMUXER_TYPE_FILM 14
#define DEMUXER_TYPE_ROQ 15
#define DEMUXER_TYPE_MF 16
#define DEMUXER_TYPE_AUDIO 17
#define DEMUXER_TYPE_OGG 18
#define DEMUXER_TYPE_RAWAUDIO 20
#define DEMUXER_TYPE_RTP 21
#define DEMUXER_TYPE_RAWDV 22
#define DEMUXER_TYPE_PVA 23
#define DEMUXER_TYPE_SMJPEG 24
#define DEMUXER_TYPE_XMMS 25
#define DEMUXER_TYPE_RAWVIDEO 26
#define DEMUXER_TYPE_MPEG4_ES 27
#define DEMUXER_TYPE_GIF 28
#define DEMUXER_TYPE_MPEG_TS 29
#define DEMUXER_TYPE_H264_ES 30
#define DEMUXER_TYPE_MATROSKA 31
#define DEMUXER_TYPE_REALAUDIO 32
#define DEMUXER_TYPE_MPEG_TY 33
#define DEMUXER_TYPE_LMLM4 34
#define DEMUXER_TYPE_LAVF 35
#define DEMUXER_TYPE_NSV 36

// This should always match the higest demuxer type number.
// Unless you want to disallow users to force the demuxer to some types
#define DEMUXER_TYPE_MIN 0
#define DEMUXER_TYPE_MAX 36

#define DEMUXER_TYPE_DEMUXERS (1<<16)
// A virtual demuxer type for the network code
#define DEMUXER_TYPE_PLAYLIST (2<<16)


#define DEMUXER_TIME_NONE 0
#define DEMUXER_TIME_PTS 1
#define DEMUXER_TIME_FILE 2
#define DEMUXER_TIME_BPS 3


// DEMUXER control commands/answers
#define DEMUXER_CTRL_NOTIMPL -1
#define DEMUXER_CTRL_DONTKNOW 0
#define DEMUXER_CTRL_OK 1
#define DEMUXER_CTRL_GUESS 2
#define DEMUXER_CTRL_GET_TIME_LENGTH 10
#define DEMUXER_CTRL_GET_PERCENT_POS 11

// Holds one packet/frame/whatever
typedef struct demux_packet_st {
	int len;
	float pts;
	off_t pos;		// position in index (AVI) or file (MPG)
	unsigned char *buffer;
	int flags;		// keyframe, etc
	int refcount;		//refcounter for the master packet, if 0, buffer can be free()d
	struct demux_packet_st *master;	//pointer to the master packet if this one is a cloned one
	struct demux_packet_st *next;
} demux_packet_t;

typedef struct {
	int buffer_pos;		// current buffer position
	int buffer_size;	// current buffer size
	unsigned char *buffer;	// current buffer, never free() it, always use free_demux_packet(buffer_ref);
	float pts;		// current buffer's pts
	int pts_bytes;		// number of bytes read after last pts stamp
	int eof;		// end of demuxed stream? (true if all buffer empty)
	off_t pos;		// position in the input stream (file)
	off_t dpos;		// position in the demuxed stream
	int pack_no;		// serial number of packet
	int flags;		// flags of current packet (keyframe etc)
//---------------
	int packs;		// number of packets in buffer
	int bytes;		// total bytes of packets in buffer
	demux_packet_t *first;	// read to current buffer from here
	demux_packet_t *last;	// append new packets from input stream to here
	demux_packet_t *current;	// needed for refcounting of the buffer
	int id;			// stream ID  (for multiple audio/video streams)
	struct demuxer_st *demuxer;	// parent demuxer structure (stream handler)
// ---- asf -----
	demux_packet_t *asf_packet;	// read asf fragments here
	int asf_seq;
// ---- mov -----
	unsigned int ss_mul, ss_div;
// ---- stream header ----
	void *sh;
} demux_stream_t;

typedef struct demuxer_info_st {
	char *name;
	char *author;
	char *encoder;
	char *comments;
	char *copyright;
} demuxer_info_t;

#define MAX_A_STREAMS 256
#define MAX_V_STREAMS 256

#include "stream.h"

typedef struct demuxer_st {
	off_t filepos;		// input stream current pos.
	off_t movi_start;
	off_t movi_end;
	stream_t *stream;
	int synced;		// stream synced (used by mpeg)
	int type;		// demuxer type: mpeg PS, mpeg ES, avi, avi-ni, avi-nini, asf
	int file_format;	// file format: mpeg/avi/asf
	int seekable;		// flag
	//
	demux_stream_t *audio;	// audio buffer/demuxer
	demux_stream_t *video;	// video buffer/demuxer
	demux_stream_t *sub;	// dvd subtitle buffer/demuxer

	// stream headers:
	void *a_streams[MAX_A_STREAMS];	// audio streams (sh_audio_t)
	void *v_streams[MAX_V_STREAMS];	// video sterams (sh_video_t)
	char s_streams[32];	// dvd subtitles (flag)

	void *priv;		// fileformat-dependent data
	char **info;

	/*
	 * ghcstop_caution add....050117
	 *
	 * read header done, demux thread때문에, 이것을 살려 놓았음.
	 * 왜냐하면 mp3나 ogg의 경우 thread 내부에서 sh_video, sh_audio를 체킹해서 없을 경우 하나만
	 돌게 해 놓았다. 그러나 avi의 경우 ds_fill_buffer를 thread가 돌기전에 먼저 호출한다.
	 이 경우 demux thread가 돌기전에는 그냥 locking이 되어 버린다. 즉, 데이터가 있기 전에는
	 ds_fill_buffer가 대기 하기 때문이다. 그러나 ds_fill_buffer가 맨처음 호출되는 시점에는
	 sh_video와 sh_audio가 결정되지 않는다.

	 그래서 ds_fill_buffer를 호출할때 sh_audio와 sh_video가 있는지 결정되기 전에는 ds_fill_buffer를
	 이전의 방식, 즉, thread bufferring방식으로 사용하지 않도록 flag를 둬서 제어한다.

	 no demux thread방식일 경우는 그냥 선언만 하고 사용하지는 않는다.
	 */

	int rhd;

#ifdef GP2X
	pthread_mutex_t dmutex;	// demux mutex, demux_fill_buffer() 동시호출 막기용. 050227
#endif
	int locked;

} demuxer_t;

inline static demux_packet_t *new_demux_packet(int len)
{
	demux_packet_t *dp = (demux_packet_t *) malloc(sizeof(demux_packet_t));
	dp->len = len;
	dp->next = NULL;
	dp->pts = 0;
	dp->pos = 0;
	dp->flags = 0;
	dp->refcount = 1;
	dp->master = NULL;
	dp->buffer = len ? (unsigned char *) malloc(len + 8) : NULL;
	if (len)
		memset(dp->buffer + len, 0, 8);
	return dp;
}

inline static void resize_demux_packet(demux_packet_t * dp, int len)
{
	if (len) {
		dp->buffer = (unsigned char *) realloc(dp->buffer, len + 8);
		memset(dp->buffer + len, 0, 8);
	} else {
		if (dp->buffer)
			free(dp->buffer);
		dp->buffer = NULL;
	}
	dp->len = len;
}

inline static demux_packet_t *clone_demux_packet(demux_packet_t * pack)
{
	demux_packet_t *dp = (demux_packet_t *) malloc(sizeof(demux_packet_t));
	while (pack->master)
		pack = pack->master;	// find the master
	memcpy(dp, pack, sizeof(demux_packet_t));
	dp->next = NULL;
	dp->refcount = 0;
	dp->master = pack;
	pack->refcount++;
	return dp;
}

inline static void free_demux_packet(demux_packet_t * dp)
{
	if (dp->master == NULL) {	//dp is a master packet
		dp->refcount--;
		if (dp->refcount == 0) {
			if (dp->buffer)
				free(dp->buffer);
			free(dp);
		}
		return;
	}
	// dp is a clone:
	free_demux_packet(dp->master);
	free(dp);
}

demux_stream_t *new_demuxer_stream(struct demuxer_st * demuxer, int id);
demuxer_t *new_demuxer(stream_t * stream, int type, int a_id, int v_id, int s_id);
void free_demuxer_stream(demux_stream_t * ds);
void free_demuxer(demuxer_t * demuxer);

inline void ds_add_packet(demux_stream_t * ds, demux_packet_t * dp);	// ghcstop ==> to inline
inline void ds_read_packet(demux_stream_t * ds, stream_t * stream, int len, float pts, off_t pos, int flags);	// ghcstop ==> to inline
inline int demux_fill_buffer(demuxer_t * demux, demux_stream_t * ds);	// ghcstop ==> to inline
inline int ds_fill_buffer(demux_stream_t * ds);	// ghcstop ==> to inline

inline static off_t ds_tell(demux_stream_t * ds)
{
	return (ds->dpos - ds->buffer_size) + ds->buffer_pos;
}

inline static int ds_tell_pts(demux_stream_t * ds)
{
	return (ds->pts_bytes - ds->buffer_size) + ds->buffer_pos;
}

inline int demux_read_data(demux_stream_t * ds, unsigned char *mem, int len);	// ghcstop ==> to inline

#if 1
#define demux_getc(ds) (\
     (ds->buffer_pos<ds->buffer_size) ? ds->buffer[ds->buffer_pos++] \
     :((!ds_fill_buffer(ds))? (-1) : ds->buffer[ds->buffer_pos++] ) )
#else
inline static int demux_getc(demux_stream_t * ds)
{
	if (ds->buffer_pos >= ds->buffer_size) {
		if (!ds_fill_buffer(ds)) {
//      printf("DEMUX_GETC: EOF reached!\n");
			return -1;	// EOF
		}
	}
//  printf("[%02X]",ds->buffer[ds->buffer_pos]);
	return ds->buffer[ds->buffer_pos++];
}
#endif

void ds_free_packs(demux_stream_t * ds);
inline int ds_get_packet(demux_stream_t * ds, unsigned char **start);	// ghcstop ==> to inline

// This is defined here because demux_stream_t ins't defined in stream.h
stream_t *new_ds_stream(demux_stream_t * ds);

static inline int avi_stream_id(unsigned int id)
{
	unsigned char *p = (unsigned char *) &id;
	unsigned char a, b;
#if WORDS_BIGENDIAN
	a = p[3] - '0';
	b = p[2] - '0';
#else
	a = p[0] - '0';
	b = p[1] - '0';
#endif
	if (a > 9 || b > 9)
		return 100;	// invalid ID
	return a * 10 + b;
}

demuxer_t *demux_open(stream_t * stream, int file_format, int aid, int vid, int sid, char *filename);
int demux_seek(demuxer_t * demuxer, float rel_seek_secs, int flags);
demuxer_t *new_demuxers_demuxer(demuxer_t * vd, demuxer_t * ad, demuxer_t * sd);

// AVI demuxer params:
extern int index_mode;		// -1=untouched  0=don't use index  1=use (geneate) index
extern char *index_file_save, *index_file_load;
extern int force_ni;
extern int pts_from_bps;

extern int extension_parsing;

int demux_info_add(demuxer_t * demuxer, char *opt, char *param);
char *demux_info_get(demuxer_t * demuxer, char *opt);
int demux_info_print(demuxer_t * demuxer);
int demux_control(demuxer_t * demuxer, int cmd, void *arg);

/* Found in demux_ogg.c */
int demux_ogg_num_subs(demuxer_t * demuxer);
int demux_ogg_sub_id(demuxer_t * demuxer, int index);
char *demux_ogg_sub_lang(demuxer_t * demuxer, int index);

#endif

extern unsigned long demuxer_get_time_length(demuxer_t * demuxer);
extern int demuxer_get_percent_pos(demuxer_t * demuxer);

extern int demuxer_type_by_filename(char *filename);
