#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "buffer.h"
#include "array.h"
#include "sys-mmap.h"

typedef struct chunk {
	enum { MEM_CHUNK, FILE_CHUNK } type;

	buffer *mem; /* either the storage of the mem-chunk or the read-ahead buffer */

	struct {
		/* filechunk */
		buffer *name; /* name of the file */
		off_t  start; /* starting offset in the file */
		off_t  length; /* octets to send from the starting offset */

		int    fd;
		struct {
			char   *start; /* the start pointer of the mmap'ed area */
			size_t length; /* size of the mmap'ed area */
			off_t  offset; /* start is <n> octet away from the start of the file */
		} mmap;

		int is_temp; /* file is temporary and will be deleted if on cleanup */
	} file;

	off_t  offset; /* octets sent from this chunk
			  the size of the chunk is either
			  - mem-chunk: mem->used - 1
			  - file-chunk: file.length
			*/

	struct chunk *next;
} chunk;

typedef struct {
	chunk *first;
	chunk *last;

	chunk *unused;
	size_t unused_chunks;

	array *tempdirs;

	off_t  bytes_in, bytes_out;
} chunkqueue;

chunkqueue *chunkqueue_init(void);
void chunkqueue_set_tempdirs(chunkqueue *cq, array *tempdirs);
void chunkqueue_append_file(chunkqueue *cq, buffer *fn, off_t offset, off_t len); /* copies "fn" */
void chunkqueue_append_mem(chunkqueue *cq, const char *mem, size_t len); /* copies memory */
void chunkqueue_append_buffer(chunkqueue *cq, buffer *mem); /* may reset "mem" */
void chunkqueue_prepend_buffer(chunkqueue *cq, buffer *mem); /* may reset "mem" */

/* functions to handle buffers to read into: */
/* return a pointer to a buffer in *mem with size *len;
 *  it should be at least min_size big, and use alloc_size if
 *  new memory is allocated.
 * modifying the chunkqueue invalidates the memory area.
 * should always be followed by chunkqueue_get_memory(),
 *  even if nothing was read.
 * pass 0 for min_size/alloc_size for default values
 */
void chunkqueue_get_memory(chunkqueue *cq, char **mem, size_t *len, size_t min_size, size_t alloc_size);
/* append first len bytes of the memory queried with
 * chunkqueue_get_memory to the chunkqueue
 */
void chunkqueue_use_memory(chunkqueue *cq, size_t len);

void chunkqueue_remove_finished_chunks(chunkqueue *cq);

void chunkqueue_steal(chunkqueue *dest, chunkqueue *src, off_t len);
struct server;
int chunkqueue_steal_with_tempfiles(struct server *srv, chunkqueue *dest, chunkqueue *src, off_t len);

off_t chunkqueue_length(chunkqueue *cq);
void chunkqueue_free(chunkqueue *cq);
void chunkqueue_reset(chunkqueue *cq);

int chunkqueue_is_empty(chunkqueue *cq);

#endif
