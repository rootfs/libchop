
#ifndef __CHOP_STREAMS_H__
#define __CHOP_STREAMS_H__

#include <chop/chop.h>
#include <chop/serializable.h>

/* Input data streams.  */

/* Declare the input stream class `chop_stream_t' (represented at run-time by
   CHOP_STREAM_CLASS) inheriting from `chop_object_t'.  */
CHOP_DECLARE_RT_CLASS (stream, object,
		       char *name;
		       size_t preferred_block_size;

		       /* common methods */
		       errcode_t (* read)  (struct chop_stream *,
					    char *, size_t, size_t *);
		       void (* close) (struct chop_stream *););


/* File stream class that inherits from `chop_stream_t'.  This declares
   CHOP_FILE_STREAM_CLASS, the object representing this class at
   run-time.  */
CHOP_DECLARE_RT_CLASS (file_stream, stream,
		       int    fd;
		       size_t size;
		       char  *map;
		       size_t position;);



/* Stream methods */

/* Return the preferred block size for reading STREAM.  */
static __inline__ size_t
chop_stream_preferred_block_size (const chop_stream_t *__stream)
{
  return (__stream->preferred_block_size);
}

/* Read at most SIZE bytes from STREAM into BUFFER.  On failure, return an
   error code (non-zero).  Otherwise, return in READ the number of bytes
   actually read.  */
static __inline__ errcode_t
chop_stream_read (chop_stream_t *__stream,
		  char *__buffer,
		  size_t __size,
		  size_t *__read)
{
  return (__stream->read (__stream, __buffer, __size, __read));
}

/* Close STREAM, i.e. deallocate any resources associated to it.  */
static __inline__ void
chop_stream_close (chop_stream_t *__stream)
{
  __stream->close (__stream);
}



/* Specific stream constructors.  */

/* Open file located at PATH and initialize STREAM as a file stream
   representing this file.  STREAM has to point to a large-enough memory area
   to hold an object whose class is CHOP_FILE_STREAM_CLASS.  */
extern errcode_t chop_file_stream_open (const char *path,
					chop_stream_t *stream);

#if 0   /* Not implemented yet */
extern errcode_t chop_ext2_stream_open (const char *path,
					const char *fs,
					chop_ext2_stream_t *stream);

extern errcode_t chop_mem_stream_open (const char *buffer,
				       size_t size,
				       chop_mem_stream_t *stream);
#endif

#endif