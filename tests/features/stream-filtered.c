/* Stack a memory stream, a zip-filtered stream, and an unzip-filtered
   stream, and make sure the output yielded is the same as the input.  */

#include <chop/chop.h>
#include <chop/streams.h>
#include <chop/filters.h>

#include <testsuite.h>


/* The input data of the source stream.  */
#define SIZE_OF_INPUT  5123123
static char input[SIZE_OF_INPUT];


int
main (int argc, char *argv[])
{
  errcode_t err;
  size_t total_read = 0, quarter = 0;
  chop_filter_t *zip_filter, *unzip_filter;
  chop_stream_t *source_stream, *zipped_stream, *unzipped_stream;

  test_init (argv[0]);
  test_init_random_seed ();

  err = chop_init ();
  test_check_errcode (err, "initializing libchop");

  {
    /* Randomize INPUT.  */
    char *p;

    for (p = input; p < input + sizeof (input); p++)
      *p = random ();
  }

  test_stage ("stacked zip/unzip filtered streams");

  source_stream = chop_class_alloca_instance (&chop_mem_stream_class);
  chop_mem_stream_open (input, sizeof (input), NULL, source_stream);

  zip_filter = chop_class_alloca_instance (&chop_zlib_zip_filter_class);
  err = chop_zlib_zip_filter_init (5, 0, zip_filter);
  test_check_errcode (err, "initializing zip filter");

  zipped_stream = chop_class_alloca_instance (&chop_filtered_stream_class);
  err = chop_filtered_stream_open (source_stream, 1,
				   zip_filter, 1,
				   zipped_stream);
  test_check_errcode (err, "initializing zip-filtered stream");

  unzip_filter = chop_class_alloca_instance (&chop_zlib_unzip_filter_class);
  err = chop_zlib_unzip_filter_init (0, unzip_filter);
  test_check_errcode (err, "initializing unzip filter");

  unzipped_stream = chop_class_alloca_instance (&chop_filtered_stream_class);
  err = chop_filtered_stream_open (zipped_stream, 1,
				   unzip_filter, 1,
				   unzipped_stream);
  test_check_errcode (err, "initializing unzip-filtered stream");

  /* Go ahead: read from UNZIPPED_STREAM and make sure we get the same data
     as in INPUT.  */
  while (1)
    {
      char buffer[4077];
      size_t read;

      err = chop_stream_read (unzipped_stream, buffer, sizeof (buffer),
			      &read);
      if (!err)
	{
	  test_assert (read <= sizeof (buffer));
	  test_assert (!memcmp (input + total_read, buffer, read));
	  total_read += read;

	  if ((total_read > sizeof (input) / 4) && (quarter == 0))
	    {
	      test_stage_intermediate ("25%%");
	      quarter++;
	    }
	  else if ((total_read > sizeof (input) / 2) && (quarter == 1))
	    {
	      test_stage_intermediate ("50%%");
	      quarter++;
	    }
	  else if ((total_read > (3 * sizeof (input)) / 4) && (quarter == 2))
	    {
	      test_stage_intermediate ("75%%");
	      quarter++;
	    }
	}

      if (err)
	break;
    }

  test_assert (err == CHOP_STREAM_END);
  test_assert (total_read == sizeof (input));

  test_stage_result (1);

  /* The magic here is that the following line destroys all the objects we've
     been using.  */
  chop_object_destroy ((chop_object_t *)unzipped_stream);

  return 0;
}

/* arch-tag: 2d70ca16-2c6c-46a0-81e7-8fe5c51a8274
 */