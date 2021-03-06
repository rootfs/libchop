/* libchop -- a utility library for distributed storage
   Copyright (C) 2008, 2010  Ludovic Courtès <ludo@gnu.org>
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

#ifndef CHOP_STORE_STATS_H
#define CHOP_STORE_STATS_H

#include <chop/chop.h>
#include <chop/logs.h>
#include <chop/stores.h>
#include <chop/objects.h>

_CHOP_BEGIN_DECLS

CHOP_DECLARE_RT_CLASS (block_store_stats, object,
		       char *name;

		       size_t blocks_written;
		       size_t bytes_written;

		       size_t virgin_blocks;
		       size_t virgin_bytes;

		       float  average_block_size;
		       size_t min_block_size;
		       size_t max_block_size;);


/* Maintaining statistics on the data written to a store.  */

extern const chop_class_t chop_stat_block_store_class;

/* Initialize STORE as an instance of CHOP_STAT_BLOCK_STORE_CLASS, a
   statistic-gathering store.  If BACKEND is not NULL, the STORE will act as
   a proxy to BACKEND.  Otherwise, it will act like a ``dummy'' store, not
   actually writing anything, and being uncapable of providing and data if
   `read_block ()' is called.  If BACKEND is not NULL, the follow the proxy
   semantics defined by BPS.  NAME is the name that will be used to identify
   the underlying block store statistics.  */
extern chop_error_t chop_stat_block_store_open (const char *name,
						chop_block_store_t *backend,
						chop_proxy_semantics_t bps,
						chop_block_store_t *store);

/* Return the statistics gathered by STAT_STORE which must be a instance of
   CHOP_STAT_BLOCK_STORE_CLASS.  Recall that the data pointed to by the
   returned pointer will be unavailable as soon as STAT_STORE is close.  */
extern const chop_block_store_stats_t *
chop_stat_block_store_stats (const chop_block_store_t *stat_store);



/* Manipulating statistics.  */

extern chop_error_t chop_block_store_stats_init (const char *name,
						 chop_block_store_stats_t
						 *stats);

extern void chop_block_store_stats_update (chop_block_store_stats_t *stats,
					   size_t block_size, int virgin_write);

extern void chop_block_store_stats_clear (chop_block_store_stats_t *stats);

extern void chop_block_store_stats_display (const chop_block_store_stats_t *,
					    chop_log_t *log);


/* Accessors.  */

static __inline__ const char *
chop_block_store_stats_name (const chop_block_store_stats_t *__stats)
{
  return (__stats->name);
}

static __inline__ size_t
chop_block_store_stats_blocks_written (const chop_block_store_stats_t *__stats)
{
  return (__stats->blocks_written);
}

static __inline__ size_t
chop_block_store_stats_bytes_written (const chop_block_store_stats_t *__stats)
{
  return (__stats->bytes_written);
}

static __inline__ size_t
chop_block_store_stats_virgin_blocks (const chop_block_store_stats_t *__stats)
{
  return (__stats->virgin_blocks);
}

static __inline__ size_t
chop_block_store_stats_virgin_bytes (const chop_block_store_stats_t *__stats)
{
  return (__stats->virgin_bytes);
}

static __inline__ float
chop_block_store_stats_average_block_size (const chop_block_store_stats_t *__stats)
{
  return (__stats->average_block_size);
}

static __inline__ size_t
chop_block_store_stats_max_block_size (const chop_block_store_stats_t *__stats)
{
  return (__stats->max_block_size);
}

static __inline__ size_t
chop_block_store_stats_min_block_size (const chop_block_store_stats_t *__stats)
{
  return (__stats->min_block_size);
}


_CHOP_END_DECLS

#endif
