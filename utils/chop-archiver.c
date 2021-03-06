/* libchop -- a utility library for distributed storage and data backup
   Copyright (C) 2008, 2010, 2011  Ludovic Courtès <ludo@gnu.org>
   Copyright (C) 2005, 2006, 2007  Centre National de la Recherche Scientifique (LAAS-CNRS)

   Libchop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Libchop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libchop.  If not, see <http://www.gnu.org/licenses/>.  */

#include <chop/chop-config.h>

#include <chop/chop.h>
#include <chop/streams.h>
#include <chop/choppers.h>
#include <chop/stores.h>
#include <chop/store-stats.h>
#include <chop/filters.h>

#include <chop/indexers.h>

#ifdef HAVE_GNUTLS
# include <gnutls/gnutls.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <argp.h>
#include <progname.h>

const char *argp_program_version = "chop-archiver (" PACKAGE_NAME ") " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] =
"chop-archiver -- archives and restores different versions of a file\
\v\
This program can archive a given file's revision into a GDBM block store \
and eventually restore it from them.  Subsequent revisions of a file will \
hopefully require less room than the sum of each revision's size.  When \
asked to archive a file (with `--archive') the program displays an \
\"archive handle\" in the form of a hash.  This handle must be kept and \
eventually passed to `--restore'.  Note that `chop-archiver' itself is \
somewhat dumb since it does not keep track of what handle correspond to \
what file or revision so you have to do this by yourself.\n\
\n\
Also, if you passed `--zip' at archival time, you will have to pass it \
at restoration time as well so that the archived stream gets decompressed \
on the fly.  Failing to do so, you will get the raw, zlib-compressed, file \
and won't be able to do anything with it (`gunzip' won't work).  The same \
goes for `--cipher'.";


/* Name of the directory for configuration files under `$HOME'.  */
#define CONFIG_DIRECTORY ".chop-archiver"

#define DB_DATA_FILE_BASE       "archive-data"
#define DB_META_DATA_FILE_BASE  "archive-meta-data"


/* Whether archival or retrieval is to be performed.  */
static int archive_queried = 0, restore_queried = 0;

/* Whether the source (when archiving) is a file descriptor.  */
static int source_is_fd = 0;

/* If ARCHIVE_QUERIED, this is the typical size of blocks that should be
   produced by the chopper.  Zero means ``chopper class preferred
   value''.  */
static size_t typical_block_size = 0;

/* The option passed to either `--archive' or `--restore'.  */
static char *option_argument = NULL;

/* The file where to store the data, if not DB_DATA_FILE_BASE and
   DB_META_DATA_FILE_BASE.  */
static char *db_file_name = NULL;

/* Use a "smart store proxy".  */
static int smart_storage = 1;

/* Use the dummy store for debugging purposes.  */
static int debugging = 0;

/* Whether to be verbose */
static int verbose = 0;

/* Zip/unzip filters to use.  */
static const chop_zip_filter_class_t   *zip_block_filter_class = NULL;
static const chop_unzip_filter_class_t *unzip_block_filter_class = NULL;
static const chop_zip_filter_class_t   *zip_stream_filter_class = NULL;
static const chop_unzip_filter_class_t *unzip_stream_filter_class = NULL;

/* Whether to show store statistics.  */
static int show_stats = 0;

/* The remote block store host name or NULL.  */
static char *remote_hostname = NULL;

/* Protocol name.  */
static char *protocol_name = "tcp";

/* Remote host port.  */
static unsigned long int service_port = 0;

#ifdef HAVE_GNUTLS
/* OpenPGP key pair for OpenPGP authentication.  */
static char *tls_openpgp_pubkey_file = NULL;
static char *tls_openpgp_privkey_file = NULL;
#define tls_use_openpgp_authentication				\
  ((tls_openpgp_privkey_file) && (tls_openpgp_pubkey_file))
#endif

#ifdef HAVE_DBUS
/* Whether to use D-BUS or SunRPC.  */
static int use_dbus = 0;
#endif


static char *file_based_store_class_name = "gdbm_block_store";
static char *chopper_class_name = "fixed_size_chopper";
static char *indexer_class_name = "tree_indexer";
static char *indexer_ascii = "64"; /* 100 keys per block */
static char *block_indexer_class_name = "hash_block_indexer";
static char *block_indexer_ascii = "SHA1";

static struct argp_option options[] =
  {
    { "verbose", 'v', 0, 0,        "Produce verbose output" },
    { "debug",   'd', 0, 0,
      "Produce debugging output and use a dummy block store (i.e. a block "
      "store that does nothing but print messages)" },
    { "no-smart-store", 'N', 0, 0,
      "Disable smart storage, which writes data blocks only when they don't "
      "already exist" },
    { "show-stats", 's', 0, 0,
      "Show statistics about the blocks that have been written (in archival "
      "mode)" },
    { "db-file", 'f', "FILE", 0,
      "Write the block database to FILE instead of the default files" },
    { "block-size", 'b', "SIZE", 0,
      "Choose a typical size of SIZE bytes for the blocks produced by "
      "the chopper" },
    { "zip-input", 'Z', "ZIP-TYPE", OPTION_ARG_OPTIONAL,
      "Pass the input stream through a zip filter to compress (resp. "
      "decompress) data when writing (resp. reading) to (resp. from) the "
      "archive.  ZIP-TYPE should be one of `zlib', `bzip2', or `lzo'." },
    { "zip",     'z', "ZIP-TYPE", OPTION_ARG_OPTIONAL,
      "Pass data blocks through a zip filter to compress (resp. decompress) "
      "data when writing (resp. reading) to (resp. from) the archive.  "
      "ZIP-TYPE should be one of `zlib', `bzip2', or `lzo'." },
    { "remote",  'R', "HOST", 0,
      "Use the remote block store located at HOST for both "
      "data and meta-data blocks; HOST may contain `:' followed by a port "
      "number" },
    { "protocol", 'p', "PROTO", 0,
      "Use PROTO (one of "
#ifdef HAVE_GNUTLS
      "\"tls/tcp\", "
#endif
      "\"tcp\", \"udp\" or \"unix\") when communicating with "
      "the remote store" },
#ifdef HAVE_GNUTLS
    { "openpgp-pubkey", 'o', "PUBKEY-FILE", 0,
      "Use PUBKEY-FILE as the OpenPGP key to be used during TLS "
      "authentication" },
    { "openpgp-privkey", 'O', "PRIVKEY-FILE", 0,
      "Use PRIVKEY-FILE as the OpenPGP key to be used during TLS "
      "authentication" },
#endif

    { "store",   'S', "CLASS", 0,
      "Use CLASS as the underlying file-based block store" },
    { "chopper", 'C', "CHOPPER", 0,
      "Use CHOPPER as the input stream chopper" },
    { "indexer-class", 'k', "I-CLASS", 0,
      "Use I-CLASS as the indexer class.  This implies `-K'." },
    { "indexer", 'K', "I", 0,
      "Deserialize I as an instance of I-CLASS and use it." },
    { "block-indexer-class", 'i', "BI-CLASS", 0,
      "Use BI-CLASS as the block-indexer class.  This implies `-I'." },
    { "block-indexer", 'I', "BI", 0,
      "Deserialize BI as an instance of BI-CLASS and use it." },

#ifdef HAVE_DBUS
    { "dbus", 'D', 0, 0,
      "Use the D-BUS rather than the SunRPC client interface. "
      "[experimental]" },
#endif

    /* The main functions.  */
    { "archive", 'a', "FILE",   0,
      "Archive FILE and return an archived revision handle" },
    { "archive-fd", 'A', "FD", OPTION_ARG_OPTIONAL,
      "Archive from FD, an open file descriptor, and return an "
      "archived revision handle" },
    { "restore", 'r', "HANDLE", 0,
      "Restore a file's revision from HANDLE, an archived revision handle" },

    { 0, 0, 0, 0, 0 }
  };



/* Archive STREAM onto DATA_STORE and METADATA_STORE.  Use CHOPPER and
   INDEXER in order to chop STREAM into blocks and then index blocks.  */
static chop_error_t
do_archive (chop_stream_t *stream, chop_block_store_t *data_store,
	    chop_block_store_t *metadata_store,
	    chop_chopper_t *chopper, chop_indexer_t *indexer)
{
  chop_error_t err = 0;
  chop_buffer_t buffer;
  chop_block_indexer_t *block_indexer;
  chop_index_handle_t *handle;

  {
    size_t bytes_read = 0;
    const chop_class_t *block_indexer_class;

    block_indexer_class = chop_class_lookup (block_indexer_class_name);
    if (!block_indexer_class)
      {
	chop_error (err, "%s: not a valid block indexer class name",
		    block_indexer_class_name);
	return err;
      }

    if (!chop_class_inherits (block_indexer_class, &chop_block_indexer_class))
      {
	chop_error (err, "%s: not a block indexer class",
		    block_indexer_class_name);
	return err;
      }

    block_indexer = chop_class_alloca_instance (block_indexer_class);
    err = chop_object_deserialize ((chop_object_t *)block_indexer,
				   block_indexer_class, CHOP_SERIAL_ASCII,
				   block_indexer_ascii,
				   strlen (block_indexer_ascii),
				   &bytes_read);
    if (err)
      {
	chop_error (err, "%s: failed to deserialize block indexer",
		    block_indexer_ascii);
	return err;
      }

    if (bytes_read < strlen (block_indexer_ascii))
      fprintf (stderr, "%s: warning: %zu: trailing garbage in block-indexer\n",
	       program_name, bytes_read);
  }

  handle = chop_block_indexer_alloca_index_handle (block_indexer);
  err = chop_indexer_index_blocks (indexer, chopper, block_indexer,
				   data_store, metadata_store, handle);
  if ((err) && (err != CHOP_STREAM_END))
    {
      chop_error (err, "while indexing blocks");
      return err;
    }

  if (verbose)
    {
      /* Take a look at the index handle we got */
      fprintf (stdout, "chop: done with indexing\n");
      fprintf (stdout, "chop: got a handle of class \"%s\"\n",
	       chop_class_name (chop_object_get_class ((chop_object_t *)handle)));
    }

  err = chop_buffer_init (&buffer, 400);
  if (err)
    exit (12);

  err = chop_ascii_serialize_index_tuple (handle, indexer, block_indexer,
					  &buffer);
  if (err)
    {
      chop_error (err, "while serializing index handle");
      exit (8);
    }

  /* Display the ASCII representation of HANDLE.  We assume that it is
     zero-terminated.  */
  if (verbose)
    fprintf (stdout, "chop: handle: %s\n", chop_buffer_content (&buffer));
  else
    /* Print the handle on a single line so that external tools can use
       it easily.  */
    fprintf (stdout, "%s\n", chop_buffer_content (&buffer));

  chop_buffer_return (&buffer);
  chop_object_destroy ((chop_object_t *)handle);
  chop_object_destroy ((chop_object_t *)block_indexer);

  if (verbose)
    fprintf (stdout, "chop: archive done\n");

  return 0;
}

/* Retrieve data pointed to by HANDLE from DATA_STORE and METADATA_STORE
   using INDEXER and display it.  */
static chop_error_t
do_retrieve (chop_index_handle_t *handle,
	     chop_indexer_t *indexer, chop_block_fetcher_t *fetcher,
	     chop_block_store_t *data_store,
	     chop_block_store_t *metadata_store)
{
  chop_error_t err;
  chop_stream_t *stream;
  char buffer[3577];  /* Dummy size chosen on purpose */

  stream = chop_indexer_alloca_stream (indexer);
  err = chop_indexer_fetch_stream (indexer, handle, fetcher,
				   data_store, metadata_store,
				   stream);
  if (err)
    {
      chop_error (err, "while retrieving stream");
      return err;
    }

  if (zip_stream_filter_class)
    {
      /* Use a unzip-filtered stream to proxy STREAM.  */
      chop_stream_t *raw_stream = stream;
      chop_filter_t *unzip_filter;

      unzip_filter = chop_class_alloca_instance ((chop_class_t *)
						 unzip_stream_filter_class);
      err = chop_unzip_filter_generic_open (unzip_stream_filter_class,
					    0, unzip_filter);
      if (err)
	{
	  chop_error (err, "while opening unzip filter");
	  exit (1);
	}

      stream = chop_class_alloca_instance (&chop_filtered_stream_class);
      err = chop_filtered_stream_open (raw_stream, 1, unzip_filter, 1,
				       stream);
      if (err)
	{
	  chop_error (err, "while opening unzip-filtered stream");
	  exit (1);
	}

      if (verbose)
	chop_log_attach (chop_filter_log (unzip_filter), 2, 0);
    }

  while (1)
    {
      size_t read = 0;
      err = chop_stream_read (stream, buffer, sizeof (buffer), &read);
      if (err)
	break;

      write (1, buffer, read);
    }

  if (err != CHOP_STREAM_END)
    {
      chop_error (err, "while reading stream");
      return err;
    }

  chop_object_destroy ((chop_object_t *)stream);

  return 0;
}

static void
log_indexer (chop_indexer_t *indexer)
{
  chop_log_t *indexer_log = NULL;

  indexer_log = chop_tree_indexer_log (indexer);
  if (indexer_log)
    /* Attach the indexer log to stderr.  */
    chop_log_attach (indexer_log, 2, 0);
}

static chop_error_t
process_command (const char *argument,
		 chop_block_store_t *data_store,
		 chop_block_store_t *metadata_store)
{
  chop_error_t err;
  chop_block_store_t *data_proxy, *metadata_proxy;
  chop_indexer_t *indexer;

  if (verbose)
    {
      data_proxy = (chop_block_store_t *)
	chop_class_alloca_instance (&chop_dummy_block_store_class);
      metadata_proxy = (chop_block_store_t *)
	chop_class_alloca_instance (&chop_dummy_block_store_class);

      chop_dummy_proxy_block_store_open ("data", data_store, data_proxy);
      chop_dummy_proxy_block_store_open ("meta-data", metadata_store,
					 metadata_proxy);
    }
  else
    data_proxy = metadata_proxy = NULL;

#define THE_STORE(_what) (_what ## _proxy ? _what ## _proxy : _what ## _store)

  if (archive_queried)
    {
      chop_stream_t *stream;
      const chop_class_t *indexer_class;
      chop_chopper_class_t *chopper_class;
      chop_chopper_t *chopper;
      size_t bytes_read;

      stream = chop_class_alloca_instance (&chop_file_stream_class);

      if (source_is_fd)
	{
	  long fd;

	  if (argument == NULL)
	    /* Default to stdin.  */
	    fd = 0;
	  else
	    {
	      char *end;

	      fd = strtol (argument, &end, 10);
	      if (end == argument || errno == ERANGE)
		{
		  fprintf (stderr,
			   "%s: failed to parse `%s' as a file descriptor\n",
			   program_name, argument);
		  exit (EXIT_FAILURE);
		}
	    }

	  err = chop_file_stream_open_fd (fd, 1, stream);
	}
      else
	err = chop_file_stream_open (argument, stream);

      if (err)
	{
	  chop_error (err, "while opening %s", argument);
	  exit (1);
	}

      if (zip_stream_filter_class)
	{
	  /* Use a zip-filtered stream to proxy STREAM.  */
	  chop_stream_t *raw_stream = stream;
	  chop_filter_t *zip_filter;

	  zip_filter = chop_class_alloca_instance ((chop_class_t *)
						   zip_stream_filter_class);
	  err =
	    chop_zip_filter_generic_open (zip_stream_filter_class,
					  CHOP_ZIP_FILTER_DEFAULT_COMPRESSION,
					  0, zip_filter);
	  if (err)
	    {
	      chop_error (err, "failed to open zip filter");
	      exit (3);
	    }

	  stream = chop_class_alloca_instance (&chop_filtered_stream_class);
	  err = chop_filtered_stream_open (raw_stream, 1, zip_filter, 1,
					   stream);
	  if (err)
	    {
	      chop_error (err, "failed to open zip-filtered input stream");
	      exit (3);
	    }

	  if (verbose)
	    chop_log_attach (chop_filter_log (zip_filter), 2, 0);
	}

      indexer_class = chop_class_lookup (indexer_class_name);
      if (!indexer_class)
	{
	  fprintf (stderr, "%s: class `%s' not found\n",
		   program_name, indexer_class_name);
	  exit (1);
	}

      if (!chop_class_inherits (indexer_class, &chop_indexer_class))
	{
	  fprintf (stderr, "%s: class `%s' is not an indexer class\n",
		   program_name, indexer_class_name);
	  exit (1);
	}

      indexer = chop_class_alloca_instance (indexer_class);
      err = chop_object_deserialize ((chop_object_t *)indexer,
				     indexer_class, CHOP_SERIAL_ASCII,
				     indexer_ascii, strlen (indexer_ascii),
				     &bytes_read);
      if (err)
	{
	  chop_error (err, "while deserializing `%s' instance",
		      indexer_class_name);
	  exit (1);
	}

      if (verbose)
	log_indexer (indexer);

      chopper_class =
	(chop_chopper_class_t *)chop_class_lookup (chopper_class_name);
      if (!chopper_class)
	{
	  fprintf (stderr, "%s: class `%s' not found\n",
		   program_name, chopper_class_name);
	  exit (1);
	}
      if (!chop_object_is_a ((chop_object_t *)chopper_class,
			     (chop_class_t *)&chop_chopper_class_class))
	{
	  fprintf (stderr, "%s: class `%s' is not a chopper class\n",
		   program_name, chopper_class_name);
	  exit (1);
	}

      chopper = chop_class_alloca_instance ((chop_class_t *)chopper_class);
      err = chop_chopper_generic_open (chopper_class, stream,
				       typical_block_size, chopper);
      if (err)
	{
	  chop_error (err, "while initializing chopper of class `%s'",
		      chopper_class_name);
	  exit (2);
	}

      err = do_archive (stream,
			THE_STORE (data), THE_STORE (metadata),
			chopper, indexer);

      chop_object_destroy ((chop_object_t *)stream);
      chop_object_destroy ((chop_object_t *)indexer);
      chop_object_destroy ((chop_object_t *)chopper);
    }
  else if (restore_queried)
    {
      size_t arg_len, bytes_read;
      chop_indexer_t *indexer;
      chop_index_handle_t *handle;
      chop_block_fetcher_t *fetcher;
      const chop_class_t *indexer_class, *fetcher_class, *handle_class;

      arg_len = strlen (argument) + 1;
      err = chop_ascii_deserialize_index_tuple_s1 (argument, arg_len,
						   &indexer_class,
						   &fetcher_class,
						   &handle_class,
						   &bytes_read);
      if (err)
	{
	  chop_error (err, "during stage 1 of the index deserialization");
	  return err;
	}

      indexer = chop_class_alloca_instance (indexer_class);
      fetcher = chop_class_alloca_instance (fetcher_class);
      handle = chop_class_alloca_instance (handle_class);

      err = chop_ascii_deserialize_index_tuple_s2 (argument + bytes_read,
						   arg_len - bytes_read,
						   indexer_class,
						   fetcher_class,
						   handle_class,
						   indexer, fetcher, handle,
						   &bytes_read);
      if (err)
	{
	  chop_error (err, "during stage 2 of the index deserialization");
	  return err;
	}

      if (verbose)
	{
	  chop_log_t *fetcher_log = chop_hash_block_fetcher_log (fetcher);

	  fetcher_log = fetcher_log ?: chop_chk_block_fetcher_log (fetcher);
	  if (fetcher_log)
	    chop_log_attach (fetcher_log, 2, 0);

	  log_indexer (indexer);
	}

      err = do_retrieve (handle, indexer, fetcher,
			 THE_STORE (data), THE_STORE (metadata));

      chop_object_destroy ((chop_object_t *)handle);
      chop_object_destroy ((chop_object_t *)fetcher);
      chop_object_destroy ((chop_object_t *)indexer);
    }
#undef THE_STORE
  else
    {
      fprintf (stderr,
	       "%s: You must pass either `--archive' or `--restore'\n",
	       program_name);
      exit (1);
    }

  if (verbose)
    {
      chop_store_close (data_proxy);
      chop_store_close (metadata_proxy);
      chop_object_destroy ((chop_object_t *)data_proxy);
      chop_object_destroy ((chop_object_t *)metadata_proxy);
    }

  return err;
}

static chop_error_t
open_db_store (const chop_file_based_store_class_t *class,
	       const char *base, chop_block_store_t *store)
{
  chop_error_t err;
  char *file, *suffix;
  const char *class_name;
  size_t file_len, home_len, base_len, suffix_len;

  base_len = strlen (base);

  class_name = chop_class_name ((chop_class_t *) class);
  suffix_len = strchr (class_name, '_') - class_name;
  suffix = alloca (suffix_len + 1);
  strncpy (suffix, class_name, suffix_len);
  suffix[suffix_len] = '\0';

  home_len = strlen (getenv ("HOME"));
  file_len = home_len + 1 + sizeof (CONFIG_DIRECTORY) + 1
    + base_len + 1 + suffix_len;
  file = alloca (file_len + 1);

  strcpy (file, getenv ("HOME"));
  strcat (file, "/");
  strcat (file, CONFIG_DIRECTORY);

  /* Create CONFIG_DIRECTORY if it doesn't already exist.  */
  if (mkdir (file, 0700))
    {
      /* When we get EEXIST, assume it's actually a directory.  */
      if (errno != EEXIST)
	{
	  chop_error (errno, "while creating directory `%s'", file);
	  exit (EXIT_FAILURE);
	}
    }

  strcat (file, "/");
  strcat (file, base);
  strcat (file, ".");
  strcat (file, suffix);

  err = chop_file_based_store_open (class, file,
				    O_RDWR | O_CREAT, S_IRUSR | S_IWUSR,
				    store);
  if (err)
    chop_error (err, "while opening `%s' data file \"%s\"",
		chop_class_name ((chop_class_t *)class), file);

  return err;
}


/* Dealing with zip/unzip filter classes.  */
#include "zip-helper.c"



/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'v':
      verbose = 1;
      break;
    case 'd':
      debugging = 1;
      break;
    case 's':
      show_stats = 1;
      break;
    case 'N':
      smart_storage = 0;
      break;
    case 'f':
      db_file_name = arg;
      break;
    case 'b':
      typical_block_size = strtoul (arg, NULL, 0);
      break;
    case 'a':
      archive_queried = 1;
      option_argument = arg;
      break;
    case 'A':
      archive_queried = 1;
      source_is_fd = 1;
      option_argument = arg;
      break;
    case 'r':
      restore_queried = 1;
      option_argument = arg;
      break;
    case 'Z':
      get_zip_filter_classes (arg, &zip_stream_filter_class,
			      &unzip_stream_filter_class);
      break;
    case 'z':
      get_zip_filter_classes (arg, &zip_block_filter_class,
			      &unzip_block_filter_class);
      break;
    case 'R':
      {
	char *colon;

	colon = strchr (arg, ':');
	if (colon)
	  {
	    char *end;
	    service_port = strtoul (colon + 1, &end, 10);
	    if (end == colon + 1)
	      {
		fprintf (stderr, "%s: %s: invalid port\n", program_name,
			 colon + 1);
		exit (1);
	      }

	    *colon = '\0';
	  }

	remote_hostname = arg;
      }
      break;
    case 'p':
      protocol_name = arg;
      break;

#ifdef HAVE_GNUTLS
    case 'o':
      tls_openpgp_pubkey_file = arg;
      break;
    case 'O':
      tls_openpgp_privkey_file = arg;
      break;
#endif

    case 'S':
      file_based_store_class_name = arg;
      break;
    case 'C':
      chopper_class_name = arg;
      break;
    case 'k':
      indexer_class_name = arg;
      break;
    case 'K':
      indexer_ascii = arg;
      break;
    case 'i':
      block_indexer_class_name = arg;
      break;
    case 'I':
      block_indexer_ascii = arg;
      break;

#ifdef HAVE_DBUS
    case 'D':
      use_dbus = 1;
      break;
#endif
    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

/* Argp argument parsing.  */
static struct argp argp = { options, parse_opt, 0, doc };


int
main (int argc, char *argv[])
{
  int failed = 0;
  chop_error_t err;
  chop_block_store_t *store, *metastore;
  chop_filter_t *input_filter = NULL, *output_filter = NULL;
  chop_cipher_handle_t cipher_handle = CHOP_CIPHER_HANDLE_NIL;

  set_program_name (argv[0]);

  /* Parse arguments.  */
  argp_parse (&argp, argc, argv, 0, 0, 0);

  err = chop_init ();
  if (err)
    {
      chop_error (err, "while initializing libchop");
      return 1;
    }

  if (!debugging)
    {
      if (remote_hostname)
	{
	  /* Use a remote block store for both data and metadata blocks.  */
#ifdef HAVE_DBUS
	  /* FIXME: We should do a generic thing the the file-based store
	     metaclass.  */
	  if (use_dbus)
	    {
	      store = (chop_block_store_t *)
		chop_class_alloca_instance (&chop_dbus_block_store_class);

	      err = chop_dbus_block_store_open (remote_hostname, store);
	    }
	  else
#endif
	    {
	      store = (chop_block_store_t *)
		chop_class_alloca_instance (&chop_sunrpc_block_store_class);

#ifdef HAVE_GNUTLS
	      if (tls_use_openpgp_authentication)
		err = chop_sunrpc_tls_block_store_simple_open
		  (remote_hostname, service_port,
		   tls_openpgp_pubkey_file, tls_openpgp_privkey_file,
		   store);
	      else
#endif
	      err = chop_sunrpc_block_store_open (remote_hostname,
						  service_port,
						  protocol_name,
						  store);
	    }

	  if (err)
	    {
	      chop_error (err, "while opening remote block store");
	      exit (3);
	    }

	  metastore = store;
	}
      else
	{
	  /* Use two GDBM stores.  */
	  const chop_file_based_store_class_t *db_store_class;

	  db_store_class = (chop_file_based_store_class_t *)
	    chop_class_lookup (file_based_store_class_name);
	  if (!db_store_class)
	    {
	      fprintf (stderr, "%s: class `%s' not found\n",
		       argv[0], file_based_store_class_name);
	      exit (1);
	    }
	  if (chop_object_get_class ((chop_object_t *)db_store_class)
	      != &chop_file_based_store_class_class)
	    {
	      fprintf (stderr,
		       "%s: class `%s' is not a file-based store class\n",
		       argv[0], file_based_store_class_name);
	      exit (1);
	    }

	  store = (chop_block_store_t *)
	    chop_class_alloca_instance ((chop_class_t *)db_store_class);
	  metastore = (chop_block_store_t *)
	    chop_class_alloca_instance ((chop_class_t *)db_store_class);

	  if (db_file_name)
	    {
	      /* We'll actually use only one database stored in the file
		 whose name was passed by the user.  */
	      err = chop_file_based_store_open (db_store_class, db_file_name,
						O_RDWR | O_CREAT,
						S_IRUSR | S_IWUSR,
						store);
	      if (err)
		{
		  chop_error (err, "%s", db_file_name);
		  exit (3);
		}

	      metastore = store;
	    }
	  else
	    {
	      /* Open the two default databases.  */
	      err = open_db_store (db_store_class, DB_DATA_FILE_BASE, store);
	      if (err)
		exit (3);

	      err = open_db_store (db_store_class, DB_META_DATA_FILE_BASE,
				   metastore);
	      if (err)
		exit (3);
	    }
	}
    }
  else
    {
      /* Use a dummy block store for debugging purposes */
      store = (chop_block_store_t *)
	chop_class_alloca_instance (&chop_dummy_block_store_class);
      metastore = (chop_block_store_t *)
	chop_class_alloca_instance (&chop_dummy_block_store_class);

      chop_dummy_block_store_open ("data", store);
      chop_dummy_block_store_open ("meta-data", metastore);
      chop_log_attach (chop_dummy_block_store_log (store), 2, 0);
      chop_log_attach (chop_dummy_block_store_log (metastore), 2, 0);
    }

  if (zip_block_filter_class)
    {
      /* Create a filtered store that uses zip filters and proxies the
	 block store for data (not metadata).  */
      chop_block_store_t *raw_store = store;

      store = chop_class_alloca_instance (&chop_filtered_block_store_class);
      input_filter = chop_class_alloca_instance ((chop_class_t *)
						 zip_block_filter_class);
      output_filter = chop_class_alloca_instance ((chop_class_t *)
						  unzip_block_filter_class);

      err = chop_zip_filter_generic_open (zip_block_filter_class,
					  CHOP_ZIP_FILTER_DEFAULT_COMPRESSION,
					  0, input_filter);
      if (!err)
	err = chop_unzip_filter_generic_open (unzip_block_filter_class,
					      0, output_filter);

      if (err)
	{
	  chop_error (err, "while initializing zip filters");
	  exit (4);
	}

      if (verbose)
	{
	  /* Dump the zip filters' logs to `stderr'.  */
	  chop_log_t *log;
	  log = chop_filter_log (input_filter);
	  chop_log_attach (log, 2, 0);
	  log = chop_filter_log (output_filter);
	  chop_log_attach (log, 2, 0);
	}

      err = chop_filtered_store_open (input_filter, output_filter,
				      raw_store,
				      CHOP_PROXY_EVENTUALLY_DESTROY,
				      store);
      if (err)
	{
	  chop_error (err, "while initializing filtered store");
	  exit (5);
	}

      if (raw_store == metastore)
	/* When the same store is used for both meta-data and data, then
	   apply the same treatment to both.  */
	metastore = store;
    }

  if (smart_storage && archive_queried)
    {
      chop_block_store_t *data_proxy, *metadata_proxy;

      data_proxy =
	chop_class_alloca_instance (&chop_smart_block_store_class);

      err = chop_smart_block_store_open (store,
					 CHOP_PROXY_EVENTUALLY_DESTROY,
					 data_proxy);
      if (!err)
	{
	  if (store != metastore)
	    {
	      metadata_proxy =
		chop_class_alloca_instance (&chop_smart_block_store_class);

	      err = chop_smart_block_store_open (metastore,
						 CHOP_PROXY_EVENTUALLY_DESTROY,
						 metadata_proxy);
	    }
	  else
	    metadata_proxy = data_proxy;
	}

      if (err)
	{
	  chop_error (err, "while creating smart store proxy");
	  exit (EXIT_FAILURE);
	}
      else
	store = data_proxy, metastore = metadata_proxy;
    }

  if (show_stats)
    {
      /* Create ``statistic stores'' proxying both block stores.  */
      chop_block_store_t *raw_store, *raw_metastore;

      raw_store = store;
      raw_metastore = metastore;

      store = chop_class_alloca_instance (&chop_stat_block_store_class);
      err = chop_stat_block_store_open ("data-store", raw_store,
					CHOP_PROXY_EVENTUALLY_DESTROY,
					store);
      if (!err)
	{
	  metastore =
	    chop_class_alloca_instance (&chop_stat_block_store_class);
	  err = chop_stat_block_store_open ("meta-data-store", raw_metastore,
					    (raw_store != raw_metastore)
					    ? CHOP_PROXY_EVENTUALLY_DESTROY
					    : CHOP_PROXY_LEAVE_AS_IS,
					    metastore);
	}

      if (err)
	{
	  chop_error (err, "while initializing stat store");
	  exit (5);
	}
    }

  /* */
  err = process_command (option_argument, store, metastore);
  if (err)
    failed = 1;

  if ((archive_queried) && (show_stats))
    {
      /* Show statistics about the blocks written by both the data store and
	 the meta-data store.  */
      chop_log_t log;
      chop_block_store_t **s;
      chop_block_store_t *the_stores[3];

      the_stores[0] = store;
      the_stores[1] = (store != metastore) ? metastore : NULL;
      the_stores[2] = NULL;

      chop_log_init ("stats", &log);
      chop_log_attach (&log, 2, 0);

      for (s = the_stores; *s; s++)
	{
	  const chop_block_store_stats_t *stats;

	  stats = chop_stat_block_store_stats (*s);
	  assert (stats);

	  chop_block_store_stats_display (stats, &log);
	}

      chop_object_destroy ((chop_object_t *)&log);
    }


  /* Destroy everything.  */

  if (cipher_handle != CHOP_CIPHER_HANDLE_NIL)
    chop_cipher_close (cipher_handle);
  if (input_filter)
    chop_object_destroy ((chop_object_t *)input_filter);
  if (output_filter)
    chop_object_destroy ((chop_object_t *)output_filter);

  err = chop_store_close ((chop_block_store_t *)store);
  if (err)
    {
      chop_error (err, "while closing output block store");
      exit (7);
    }
  chop_object_destroy ((chop_object_t *)store);

  if (store != metastore)
    {
      err = chop_store_close ((chop_block_store_t *)metastore);
      if (err)
	{
	  chop_error (err, "while closing output meta-data block store");
	  exit (7);
	}
      chop_object_destroy ((chop_object_t *)metastore);
    }

  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
