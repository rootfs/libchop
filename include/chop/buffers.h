
#ifndef __CHOP_BUFFERS_H__
#define __CHOP_BUFFERS_H__

#include <chop/chop.h>
#include <string.h>

typedef struct chop_buffer chop_buffer_t;

struct chop_buffer
{
  char *buffer;
  size_t real_size;
  size_t size;
};


/* Initialize BUFFER with an initial buffer of SIZE bytes.  */
extern errcode_t chop_buffer_init (chop_buffer_t *buffer,
				   size_t size);

/* Overwrite BUFFER's contents by pushing in the data from BUF which is SIZE
   byte long.  */
extern errcode_t chop_buffer_push (chop_buffer_t *buffer,
				   const char *buf, size_t size);

/* Append BUF which is SIZE byte long to BUFFER.  */
extern errcode_t chop_buffer_append (chop_buffer_t *buffer,
				     const char *buf, size_t size);

/* Return the size (in bytes) of BUFFER's contents.  */
static __inline__ size_t chop_buffer_size (const chop_buffer_t *__buffer)
{
  return (__buffer->size);
}

/* Clear BUFFER's content.  */
static __inline__ void chop_buffer_clear (chop_buffer_t *__buffer)
{
  __buffer->size = 0;
}

/* Return the underlying buffer.  */
static __inline__ const char *
chop_buffer_content (const chop_buffer_t *__buffer)
{
  return (__buffer->buffer);
}

static __inline__ void chop_buffer_copy (const chop_buffer_t *buffer,
					 char *dest,
					 size_t size)
{
  memcpy (dest, buffer->buffer, size);
}

/* Return BUFFER to its owner for deallocation.  */
extern void chop_buffer_return (chop_buffer_t *buffer);

#endif
