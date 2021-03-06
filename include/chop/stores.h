/* libchop -- a utility library for distributed storage
   Copyright (C) 2008, 2010, 2012  Ludovic Courtès <ludo@gnu.org>
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


#ifndef CHOP_STORES_H
#define CHOP_STORES_H

/* Block stores.  */

#include <chop/chop.h>
#include <chop/buffers.h>
#include <chop/objects.h>
#include <chop/logs.h>

#include <unistd.h>
#include <stdbool.h>

struct chop_block_iterator;

struct chop_block_key
{
  char  *key;
  size_t size;
  void (* dispose) (char *, void *);
  void *owner;
};

typedef struct chop_block_key chop_block_key_t;

/* Declare `chop_block_store_t' (represented at run-time by
   CHOP_BLOCK_STORE_CLASS) as inheriting from `chop_object_t'.  */
CHOP_DECLARE_RT_CLASS (block_store, object,
		       char *name;

		       chop_error_t (* blocks_exist) (struct
						      chop_block_store *,
						      size_t,
						      const
						      chop_block_key_t keys[],
						      bool exists[]);

		       chop_error_t (* read_block) (struct chop_block_store *,
						    const chop_block_key_t *,
						    chop_buffer_t *, size_t *);
		       chop_error_t (* write_block) (struct chop_block_store *,
						     const chop_block_key_t *,
						     const char *, size_t);
		       chop_error_t (* delete_block) (struct
						      chop_block_store *,
						      const chop_block_key_t *);

		       const chop_class_t *iterator_class;
		       chop_error_t (* first_block) (struct chop_block_store *,
						     struct
						     chop_block_iterator *);
		       chop_error_t (* close) (struct chop_block_store *);
		       chop_error_t (* sync) (struct chop_block_store *););




/* Block keys.  */

static __inline__ void chop_block_key_init (chop_block_key_t *__key,
					    char *__key_contents,
					    size_t __key_size,
					    void (* __dispose_func) (char *,
								     void *),
					    void *__user_data)
{
  __key->key = __key_contents;
  __key->size = __key_size;
  __key->dispose = __dispose_func;
  __key->owner = __user_data;
}

static __inline__ void chop_block_key_free (chop_block_key_t *__key)
{
  if (__key->dispose)
    __key->dispose (__key->key, __key->owner);
  __key->key = NULL;
  __key->size = 0;
  __key->dispose = NULL;
  __key->owner = NULL;
}

static __inline__ size_t chop_block_key_size (const chop_block_key_t *__key)
{
  return (__key->size);
}

static __inline__ const char *
chop_block_key_buffer (const chop_block_key_t *__key)
{
  return (__key->key);
}

/* Convert key KEY into an hexadecimal representation stored in HEX.  HEX has
   to be at least twice as long as KEY's buffer plus one byte.  */
static __inline__ void
chop_block_key_to_hex_string (const chop_block_key_t *__key,
			      char *__hex)
{
  chop_buffer_to_hex_string (chop_block_key_buffer (__key),
			     chop_block_key_size (__key),
			     __hex);
}

/* Return true (non-zero) if keys KEY1 and KEY2 are equal.  */
static __inline__ int
chop_block_key_equal (const chop_block_key_t *__key1,
		      const chop_block_key_t *__key2)
{
  return ((chop_block_key_size (__key1) == chop_block_key_size (__key2))
	  && (!memcmp (chop_block_key_buffer (__key1),
		       chop_block_key_buffer (__key2),
		       chop_block_key_size (__key1))));
}


/* Block iterators.  */

CHOP_DECLARE_RT_CLASS (block_iterator, object,
		       chop_error_t (* next) (struct chop_block_iterator *);

		       int nil;
		       chop_block_store_t *store;
		       chop_block_key_t key;);

/* Return true (non-zero) if IT is the nil iterator.  */
static __inline__ int
chop_block_iterator_is_nil (chop_block_iterator_t *__it)
{
  return (__it->nil);
}

/* Return the key corresponding to the block iterator IT, assuming IT is not
   nil.  */
static __inline__ const chop_block_key_t *
chop_block_iterator_key (const chop_block_iterator_t *__it)
{
  return (&__it->key);
}

/* Update block iterator IT so that it points to the next block.  On success,
   zero is returned.  If no next block is available, CHOP_STORE_END is
   returned and IT becomes nil.  */
static __inline__ chop_error_t
chop_block_iterator_next (chop_block_iterator_t *__it)
{
  return __it->next (__it);
}



/* Implementations of the block store interface.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* The class of all the file-based block store classes (i.e. GDBM, TDB,
   etc.).  Provides a generic method for opening such a store that is called
   by `chop_file_based_store_open ()'.  This declares
   CHOP_FILE_BASED_STORE_CLASS_CLASS and `chop_file_based_store_class_t'.  */
CHOP_DECLARE_RT_CLASS (file_based_store_class, klass,
		       chop_error_t (* generic_open) (const
						      chop_class_t *klass,
						      const char *file,
						      int open_flags,
						      mode_t mode,
						      chop_block_store_t
						      *store););


extern const chop_class_t chop_dummy_block_store_class;
extern const chop_file_based_store_class_t chop_gdbm_block_store_class;
extern const chop_file_based_store_class_t chop_tdb_block_store_class;
extern const chop_file_based_store_class_t chop_bdb_block_store_class;
extern const chop_file_based_store_class_t chop_qdbm_block_store_class;
extern const chop_file_based_store_class_t chop_fs_block_store_class;
extern const chop_class_t chop_sunrpc_block_store_class;
extern const chop_class_t chop_dbus_block_store_class;
extern const chop_class_t chop_smart_block_store_class;


/* Initialize STORE as a "dummy" block store that does nothing but display
   what goes on.  Only useful for debugging purposes.  STORE must point to
   enough memory to store an instance of type
   CHOP_DUMMY_BLOCK_STORE_CLASS.  */
extern void chop_dummy_block_store_open (const char *name,
					 chop_block_store_t *store);

/* Open a "dummy" block store that acts as a proxy of BACKEND.  This allows
   to display accesses to BACKEND.  All operations but CHOP_STORE_CLOSE are
   forwarded to BACKEND.  */
extern void
chop_dummy_proxy_block_store_open (const char *name,
				   chop_block_store_t *backend,
				   chop_block_store_t *store);

/* Return the log of STORE.  This may be used to change the way the dummy
   block store logs its data (by default, its log is attached to the standard
   error output).  */
extern chop_log_t *chop_dummy_block_store_log (chop_block_store_t *store);

/* Open GDBM database file NAME, with mode MODE (same as for open(2) and
   chmod(2)). OPEN_FLAGS are flags as those passed to open(2).  If BLOCK_SIZE
   is lower than 512, the use the filesystem block size as the GDBM size for
   block transferts, otherwise use the value of BLOCK_SIZE.  FATAL_FUNC will
   be called by GDBM whenever a fatal error occurs; if NULL, a built-in GDBM
   function will be called.  On success, return zero and initialize the
   object pointed to by STORE.  STORE must point to enough memory to store an
   instance of type CHOP_GDBM_BLOCK_STORE_CLASS.  */
extern chop_error_t chop_gdbm_store_open (const char *name, size_t block_size,
					  int open_flags, mode_t mode,
					  void (* fatal_func) (const char *),
					  chop_block_store_t *store);

/* Open TDB (the Trivial Database) file NAME, etc, etc.  Availability of this
   function depends on whether you had TDB installed at compilation time.
   TDB databases are usually slightly smaller than GDBM ones.  */
extern chop_error_t chop_tdb_store_open (const char *name,
					 int hash_size, int tdb_flags,
					 int open_flags, mode_t mode,
					 chop_block_store_t *store);

/* Open a SleepyCat BDB store under file NAME.  DB_TYPE should be a BDB
   database type, e.g., `DB_HASH'.  The other parameters are just
   as usual. */
extern chop_error_t chop_bdb_store_open (const char *name, int db_type,
					 int open_flags, mode_t mode,
					 chop_block_store_t *s);

/* Same as `chop_gdbm_store_open ()' for a QDBM database.  */
extern chop_error_t chop_qdbm_store_open (const char *name, size_t block_size,
					  int open_flags, mode_t mode,
					  chop_block_store_t *store);

/* XXX:  Implement a store for SkipDB,
   http://www.dekorte.com/projects/opensource/SkipDB/ .  */

/* Open a file system based block store rooted at DIR_FD, an open file
   descriptor to a directory.  On success, return 0 and initialize STORE.  If
   EVENTUALLY_CLOSE is non-zero, close DIR_FD when the returned store is
   closed or destroyed.  */
extern chop_error_t chop_fs_store_open (int dir_fd,
					int eventually_close,
					chop_block_store_t *store);

/* This function is a simple version of the GDBM/TDB block store open
   functions which it just calls.  The first argument gives the pointer to
   one of the database-based block store classes.  */
extern chop_error_t
chop_file_based_store_open (const chop_file_based_store_class_t *klass,
			    const char *file, int open_flags, mode_t mode,
			    chop_block_store_t *store);

/* Open a remote block store located at HOST on port PORT with protocol
   PROTOCOL.  If PORT is zero, then the remote portmap service is consulted
   to find out the relevant port number.  PROTOCOL may be either "unix",
   "udp", "tcp" or "tls/tcp" (TLS over TPC, with anonymous authentication).
   When PROTOCOL is "unix", HOST is interpreted as a named socket path and
   PORT is ignored.  Return 0 on success.  */
extern chop_error_t
chop_sunrpc_block_store_open (const char *host, unsigned port,
			      const char *protocol,
			      chop_block_store_t *store);

/* Open a remote block store using TLS over TPC.  If PUBKEY_FILE and
   PRIVKEY_FILE are non-null, then they are assumed to contain resp. an
   OpenPGP public key and an OpenPGP private key which are to be used during
   the TLS authentication; otherwise, anonymous authentication is used.  */
extern chop_error_t
chop_sunrpc_tls_block_store_simple_open (const char *host, unsigned port,
					 const char *pubkey_file,
					 const char *privkey_file,
					 chop_block_store_t *store);

/* EXPERIMENTAL!

   Initialize STORE as a D-BUS-based remote block store located at
   DBUS_ADDRESS.  DBUS_ADDRESS must be a D-BUS address string, such as
   `tcp:host=localhost,port=7777'.  */
extern chop_error_t
chop_dbus_block_store_open (const char *dbus_address,
			    chop_block_store_t *store);

/* Initialize STORE as a ``smart proxy'' of BACKEND, meaning that STORE will
   only forward `write_block' requests to BACKEND is the block doesn't
   already exist in BACKEND.  This is particularly useful as a proxy to
   remote block stores.  BPS specifies how STORE will behave as a proxy to
   BACKEND.  */
extern chop_error_t chop_smart_block_store_open (chop_block_store_t *backend,
						 chop_proxy_semantics_t bps,
						 chop_block_store_t *store);

/* Return the log attached to STORE, a smart block store.  If STORE is not an
   instance of CHOP_SMART_BLOCK_STORE_CLASS, then NULL is returned.  */
extern chop_log_t *chop_smart_block_store_log (chop_block_store_t *store);


/* XXX: We might want to have a look at Berkeley DB (`libdb3'), or even the
   TDB Replication System (http://tdbrepl.inodes.org/) or a DHT.  */


/* The filtered block store class.  */

#include <chop/filters.h>

extern const chop_class_t chop_filtered_block_store_class;

/* Initialize STORE as a filtered block store which uses INPUT_FILTER to
   filter the contents of blocks that are written to it, OUTPUT_FILTER to
   filter the contents of blocks as they are read from it, and uses BACKEND
   as the underlying block store.  BPS specify how STORE should behave as a
   proxy of BACKEND.  */
extern chop_error_t chop_filtered_store_open (chop_filter_t *input_filter,
					      chop_filter_t *output_filter,
					      chop_block_store_t *backend,
					      chop_proxy_semantics_t bps,
					      chop_block_store_t *store);



/* The block store interface.  */

/* Check whether the N blocks with keys KEYS are available in STORE.  On
   success, zero is returned and EXISTS[i] is set to zero if nothing is
   available under KEY[i], non-zero otherwise.  */
static __inline__ chop_error_t
chop_store_blocks_exist (chop_block_store_t *store,
			 size_t n, const chop_block_key_t keys[],
			 bool exists[])
{
  if (store->blocks_exist)
    return (store->blocks_exist (store, n, keys, exists));

  return CHOP_ERR_NOT_IMPL;
}

/* Same as above, but with a single block.  */
static __inline__ chop_error_t
chop_store_block_exists (chop_block_store_t *store,
			 const chop_block_key_t *key,
			 int *exists)
{
  *exists = 0;
  return chop_store_blocks_exist (store, 1, key, (bool *) exists);
}

/* Store into BUFFER the data stored under key KEY in STORE.  On success,
   return zero and set *SIZE to the size in bytes of the data read.  */
static __inline__ chop_error_t
chop_store_read_block (chop_block_store_t *__store,
		       const chop_block_key_t *__key,
		       chop_buffer_t *__buffer, size_t *__size)
{
  return (__store->read_block (__store, __key, __buffer, __size));
}


/* Write the SIZE bytes pointed to by BLOCK under key KEY in STORE.  Return
   zero on success.  */
static __inline__ chop_error_t
chop_store_write_block (chop_block_store_t *__store,
			const chop_block_key_t *__key,
			const char *__block,
			size_t __size)
{
  return (__store->write_block (__store, __key, __block, __size));
}

/* Delete the block store under KEY from STORE.  If no data was stored under
   KEY in STORE then CHOP_STORE_BLOCK_UNAVAIL is returned.  */
static __inline__ chop_error_t
chop_store_delete_block (chop_block_store_t *__store,
			 const chop_block_key_t *__key)
{
  if (__store->delete_block)
    return (__store->delete_block (__store, __key));

  return CHOP_ERR_NOT_IMPL;
}

/* Return the block iterator class associated with the class of STORE.  This
   may be NULL if STORE does not implement sequential access.  */
static __inline__ const chop_class_t *
chop_store_iterator_class (const chop_block_store_t *__store)
{
  return (__store->iterator_class);
}

/* Return in IT, an uninitialized block iterator object (IT must point to an
   area large enough for instances of the iterator class associated to
   STORE's class), an iterator to the very first object available in STORE.
   On success, zero is returned and the user shall eventually destroy IT via
   `chop_object_destroy ()'.  If STORE is empty, CHOP_STORE_END is returned.
   If STORE does not implement sequential block access, CHOP_ERR_NOT_IMPL is
   returned.  */
static __inline__ chop_error_t
chop_store_first_block (chop_block_store_t *__store,
			chop_block_iterator_t *__it)
{
  if (__store->first_block)
    return (__store->first_block (__store, __it));

  return CHOP_ERR_NOT_IMPL;
}


static __inline__ chop_error_t chop_store_sync (chop_block_store_t *__store)
{
  return (__store->sync (__store));
}

static __inline__ chop_error_t chop_store_close (chop_block_store_t *__store)
{
  return (__store->close (__store));
}

#endif
