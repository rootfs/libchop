/* Support functions for store browsers and in particular callbacks to Scheme
   code.  */

#include <chop/objects.h>
#include <chop/store-browsers.h>

#include "core-support.h"


CHOP_DECLARE_RT_CLASS_WITH_METACLASS (scheme_store_browser, store_browser,
				      hybrid_scheme_class, /* metaclass */

				      chop_store_browser_t *backend;
				      SCM discovery_proc;
				      SCM removal_proc;);


/* Discovery/removal callback trampolines.  */

static int
ssb_discovery_trampoline (chop_store_browser_t *browser,
			  const char *service_name,
			  const char *host,
			  unsigned port,
			  chop_hash_method_spec_t hash_spec,
			  const chop_class_t *client_class,
			  void *userdata)
{
  SCM result;
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)userdata;
  result = scm_call_3 (scm->discovery_proc,
		       scm_from_locale_string (service_name),
		       scm_from_locale_string (host),
		       scm_from_uint (port));
  if (scm_is_true (result))
    return 1;

  return 0;
}

static int
ssb_removal_trampoline (chop_store_browser_t *browser,
			const char *service_name,
			void *userdata)
{
  SCM result;
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)userdata;
  result = scm_call_1 (scm->removal_proc,
		       scm_from_locale_string (service_name));
  if (scm_is_true (result))
    return 1;

  return 0;
}

static errcode_t
ssb_iterate (chop_store_browser_t *browser, unsigned msecs)
{
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)browser;
  if (!scm->backend)
    return CHOP_INVALID_ARG;
  else
    return chop_store_browser_iterate (scm->backend, msecs);
}

static errcode_t
ssb_loop (chop_store_browser_t *browser)
{
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)browser;
  if (!scm->backend)
    return CHOP_INVALID_ARG;
  else
    return chop_store_browser_loop (scm->backend);
}


/* The constructor.  */

static errcode_t
chop_avahi_store_browser_open_alloc (const char *domain,
				     SCM discovery, SCM removal,
				     chop_store_browser_t **browser)
{
  errcode_t err;
  chop_store_browser_t *backend;
  chop_scheme_store_browser_t *scm;

  *browser = NULL;
  backend =
    scm_malloc (chop_class_instance_size (&chop_avahi_store_browser_class));
  scm =
    scm_malloc (chop_class_instance_size ((chop_class_t *)&chop_scheme_store_browser_class));

  err = chop_avahi_store_browser_open (domain,
				       ssb_discovery_trampoline, scm,
				       ssb_removal_trampoline, scm,
				       backend);
  if (err)
    {
      free (scm);
      free (backend);
      return err;
    }

  err =
    chop_object_initialize ((chop_object_t *)scm,
			    (chop_class_t *)&chop_scheme_store_browser_class);
  if (err)
    {
      chop_object_destroy ((chop_object_t *)backend);
      free (scm);
      free (backend);
      return err;
    }

  *browser = (chop_store_browser_t *)scm;

  scm->store_browser.loop = ssb_loop;
  scm->store_browser.iterate = ssb_iterate;
  scm->store_browser.discovery_data = NULL;
  scm->store_browser.removal_data = NULL;

  scm->backend = backend;
  scm->discovery_proc = discovery;
  scm->removal_proc = removal;

  return err;
}


/* Bookkeeping of the `chop_scheme_store_browser_t' objects.  */

static void
ssb_dtor (chop_object_t *object)
{
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)object;
  if (scm->backend)
    {
      chop_object_destroy ((chop_object_t *)scm->backend);
      free (scm->backend);
      scm->backend = NULL;
    }
}

/* Mark the closures embedded into our C object.  */
static SCM
ssb_mark (chop_object_t *object)
{
  chop_scheme_store_browser_t *scm;

  scm = (chop_scheme_store_browser_t *)object;
  scm_gc_mark (scm->discovery_proc);

  return (scm->removal_proc);
}

CHOP_DEFINE_RT_CLASS_WITH_METACLASS (scheme_store_browser, store_browser,
				     hybrid_scheme_class, /* metaclass */

				     /* metaclass inits */
				     .mark = ssb_mark,

				     NULL, ssb_dtor,
				     NULL, NULL,
				     NULL, NULL);


/* arch-tag: 7b99f655-0706-4f09-9ea6-add4556a8cdc
 */