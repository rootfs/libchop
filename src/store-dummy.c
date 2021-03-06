/* libchop -- a utility library for distributed storage and data backup
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

/* A dummy block store for debugging purposes.  */

#include <chop/chop-config.h>

#include <alloca.h>

#include <chop/chop.h>
#include <chop/stores.h>
#include <chop/buffers.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* Class definitions.  */

CHOP_DECLARE_RT_CLASS (dummy_block_store, block_store,
		       chop_log_t log;
		       chop_block_store_t *backend;);

static void
dbs_dtor (chop_object_t *object)
{
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)object;

  if (dummy->block_store.name)
    chop_free (dummy->block_store.name,
	       &chop_dummy_block_store_class);
  dummy->block_store.name = NULL;

  chop_object_destroy ((chop_object_t *)&dummy->log);
}

CHOP_DEFINE_RT_CLASS (dummy_block_store, block_store,
		      NULL, dbs_dtor,
		      NULL, NULL,
		      NULL, NULL  /* No serializer/deserializer */);




static chop_error_t
chop_dummy_block_store_blocks_exist (chop_block_store_t *store,
				     size_t n, const chop_block_key_t keys[n],
				     bool exists[n])
{
  chop_error_t err;
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_log_printf (&dummy->log,
		   "dummy: blocks_exist (%s@%p, %zi keys)",
		   store->name, store, n);
  if (!dummy->backend)
    {
      memset (exists, 0, sizeof exists);
      return 0; /* CHOP_ERR_NOT_IMPL ? */
    }

  err = chop_store_blocks_exist (dummy->backend, n, keys, exists);

  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: blocks_exist: underlying store returned \"%s\"\n",
		     chop_error_message (err));
  else
    {
      size_t i;
      char hex_key[1024];

      for (i = 0; i < n; i++)
	{
	  chop_block_key_to_hex_string (&keys[i], hex_key);
	  chop_log_printf (&dummy->log,
			   "dummy: block 0x%s does %sexist",
			   hex_key, (exists[i] ? "" : "NOT "));
	}
    }

  return err;
}

static chop_error_t
chop_dummy_block_store_read_block (chop_block_store_t *store,
				   const chop_block_key_t *key,
				   chop_buffer_t *buffer,
				   size_t *size)
{
  chop_error_t err;
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;
  char hex_key[1024];

  chop_block_key_to_hex_string (key, hex_key);
  chop_log_printf (&dummy->log,
		   "dummy: read_block (%s@%p, 0x%s,\n"
		   "                   %p, %p)\n",
		   store->name, store, hex_key, buffer, size);
  *size = 0;

  if (!dummy->backend)
    return CHOP_ERR_NOT_IMPL;

  err = chop_store_read_block (dummy->backend, key, buffer, size);

  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: read_block: underlying store returned \"%s\"\n",
		     chop_error_message (err));

  if ((!err) && (chop_buffer_size (buffer) != *size))
    chop_log_printf (&dummy->log,
		     "dummy: read_block: warning: buffer size is %zu while "
		     "reported size is %zu\n",
		     chop_buffer_size (buffer), *size);

  return err;
}

static chop_error_t
chop_dummy_block_store_write_block (chop_block_store_t *store,
				    const chop_block_key_t *key,
				    const char *block, size_t size)
{
  chop_error_t err;
  char hex_key[1024];
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_block_key_to_hex_string (key, hex_key);
  chop_log_printf (&dummy->log,
		   "dummy: write_block (%s@%p, 0x%s,\n"
		   "                    %p, %zu)\n",
		   store->name, store, hex_key, block, size);

  if (!dummy->backend)
    return 0;

  err = chop_store_write_block (dummy->backend, key, block, size);
  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: write_block: underlying store returned \"%s\"\n",
		     chop_error_message (err));

  return err;
}

static chop_error_t
chop_dummy_block_store_delete_block (chop_block_store_t *store,
				     const chop_block_key_t *key)
{
  chop_error_t err;
  char hex_key[1024];
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_block_key_to_hex_string (key, hex_key);
  chop_log_printf (&dummy->log,
		   "dummy: delete_block (%s@%p, 0x%s,\n",
		   store->name, store, hex_key);

  if (!dummy->backend)
    return 0;

  err = chop_store_delete_block (dummy->backend, key);
  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: delete_block: underlying store returned \"%s\"\n",
		     chop_error_message (err));

  return err;
}

static chop_error_t
chop_dummy_block_store_first_block (chop_block_store_t *store,
				    chop_block_iterator_t *it)
{
  chop_error_t err;
  char hex_key[1024];
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  if ((!dummy->backend) || (!chop_store_iterator_class (store)))
    return CHOP_ERR_NOT_IMPL;

  err = chop_store_first_block (dummy->backend, it);
  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: first_block: underlying store returned \"%s\"\n",
		     chop_error_message (err));
  else
    {
      const chop_block_key_t *key;

      key = chop_block_iterator_key (it);
      chop_block_key_to_hex_string (key, hex_key);
      chop_log_printf (&dummy->log,
		       "dummy: first_block (%s@%p) => 0x%s",
		       store->name, store, hex_key);
    }

  return err;
}


static chop_error_t
chop_dummy_block_store_sync (chop_block_store_t *store)
{
  chop_error_t err;
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_log_printf (&dummy->log,
		   "dummy: sync (%s@%p)\n", store->name, store);
  if (!dummy->backend)
    return 0;

  err = chop_store_sync (dummy->backend);
  if (err)
    chop_log_printf (&dummy->log,
		     "dummy: sync: underlying store returned \"%s\"\n",
		     chop_error_message (err));

  return err;
}

static chop_error_t
chop_dummy_block_store_close (chop_block_store_t *store)
{
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_log_printf (&dummy->log,
		   "dummy: close (%s@%p)\n", store->name, store);

  if (dummy->backend)
    {
      chop_store_close (dummy->backend);
      dummy->backend = NULL;
    }

  return 0;
}

void
chop_dummy_block_store_open (const char *name,
			     chop_block_store_t *store)
{
  chop_error_t err;
  char *log_name;
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  log_name = alloca (strlen (name) + 6 + 1);
  strcpy (log_name, "dummy/");
  strcpy (log_name + 6, name);

  err = chop_object_initialize ((chop_object_t *)store,
				&chop_dummy_block_store_class);
  if (err)
    return;  /* FIXME */

  err = chop_log_init (log_name, &dummy->log);
  if (err)
    {
      chop_object_destroy ((chop_object_t *)store);
      return; /* FIXME */
    }

  /* By default, dump to stderr */
/*   chop_log_attach (&dummy->log, 2, 0); */

  store->name = chop_strdup (name, &chop_dummy_block_store_class);
  store->iterator_class = NULL; /* not supported */
  store->blocks_exist = chop_dummy_block_store_blocks_exist;
  store->read_block = chop_dummy_block_store_read_block;
  store->write_block = chop_dummy_block_store_write_block;
  store->delete_block = chop_dummy_block_store_delete_block;
  store->first_block = chop_dummy_block_store_first_block;
  store->close = chop_dummy_block_store_close;
  store->sync = chop_dummy_block_store_sync;

  dummy->backend = NULL;
}

void
chop_dummy_proxy_block_store_open (const char *name,
				   chop_block_store_t *backend,
				   chop_block_store_t *store)
{
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  chop_dummy_block_store_open (name, store);

  dummy->backend = backend;
  if (backend)
    /* Note: since we do not implement a proxy iterator class here, we are
       not able to track calls to `chop_block_iterator_next ()'.  */
    store->iterator_class = chop_store_iterator_class (backend);
}

chop_log_t *
chop_dummy_block_store_log (chop_block_store_t *store)
{
  chop_dummy_block_store_t *dummy =
    (chop_dummy_block_store_t *)store;

  return (&dummy->log);
}
