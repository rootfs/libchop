#include <chop/chop.h>
#include <chop/choppers.h>
#include <chop/streams.h>

#include <alloca.h>
#include <errno.h>


/* Class definitions.  */

CHOP_DEFINE_RT_CLASS (chopper, object,
		      NULL, NULL, /* No constructor/destructor */
		      NULL, NULL  /* No serializer/deserializer */);

CHOP_DEFINE_RT_CLASS (fixed_size_chopper, chopper,
		      NULL, NULL, /* No constructor/destructor */
		      NULL, NULL  /* No serializer/deserializer */);


static errcode_t chop_fixed_chopper_read_block (chop_chopper_t *,
						chop_buffer_t *block,
						size_t *);

errcode_t
chop_fixed_size_chopper_init (chop_stream_t *input,
			      size_t block_size,
			      int pad_blocks,
			      chop_fixed_size_chopper_t *chopper)
{
  chop_object_initialize ((chop_object_t *)chopper,
			  &chop_fixed_size_chopper_class);

  chopper->chopper.stream = input;
  chopper->chopper.read_block = chop_fixed_chopper_read_block;
  chopper->chopper.typical_block_size = block_size;
  chopper->chopper.close = NULL;

  chopper->block_size = block_size;
  chopper->pad_blocks = pad_blocks;

  return 0;
}

static errcode_t
chop_fixed_chopper_read_block (chop_chopper_t *chopper,
			       chop_buffer_t *buffer,
			       size_t *size)
{
  errcode_t err = 0;
  size_t amount;
  char *block;
  chop_fixed_size_chopper_t *fixed = (chop_fixed_size_chopper_t *)chopper;
  chop_stream_t *input = chop_chopper_stream (chopper);

  block = (char *)alloca (fixed->block_size);
  if (!block)
    return ENOMEM;

  *size = 0;
  while (*size < fixed->block_size)
    {
      amount = 0;
      err = chop_stream_read (input, &block[*size],
			      fixed->block_size, &amount);
      *size += amount;

      if (err)
	{
	  if (err == CHOP_STREAM_END)
	    break;
	  else
	    return err;
	}
    }

  if (*size == 0)
    /* Tried to read past the end of stream */
    return err;

  if ((fixed->pad_blocks) && (*size < fixed->block_size))
    {
      /* Reached the end of stream: pad block with zeros */
      memset (&block[*size], '0', fixed->block_size - *size);
      *size = fixed->block_size;
    }

  err = chop_buffer_push (buffer, block, *size);

  return err;
}