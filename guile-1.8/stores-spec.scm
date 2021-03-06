;;; libchop -- a utility library for distributed storage and data backup
;;; Copyright (C) 2008, 2010  Ludovic Courtès <ludo@gnu.org>
;;; Copyright (C) 2005, 2006, 2007  Centre National de la Recherche Scientifique (LAAS-CNRS)
;;;
;;; Libchop is free software: you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation, either version 3 of the License, or
;;; (at your option) any later version.
;;;
;;; Libchop is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with libchop.  If not, see <http://www.gnu.org/licenses/>.

(define-module (stores-spec)
  #:use-module (core-spec)
  #:use-module (filters-spec)

  #:use-module (oop goops)
  #:use-module (srfi srfi-1)

  #:use-module (g-wrap)
  #:use-module (g-wrap c-codegen)
  #:use-module (g-wrap rti)
  #:use-module (g-wrap c-types)
  #:use-module (g-wrap ws standard)

  ;; Guile-specific things
  #:use-module (g-wrap guile)
  #:use-module (g-wrap guile ws standard)

  #:export (<chop-store-wrapset>))


;;;
;;; Wrapset.
;;;

(define-class <chop-store-wrapset> (<gw-guile-wrapset>)
  #:id 'stores
  #:dependencies '(standard core filters))



;;;
;;; Type wrapping customization.
;;;

;; Define block key as a variant of non-writable input buffers.
(define-class <chop-block-key-type> (<gw-type>))

(define-method (check-typespec-options (type <chop-block-key-type>)
				       (options <list>))
  #t)

(define-method (c-type-name (type <chop-block-key-type>)
			    (typespec <gw-typespec>))
  "chop_block_key_t")

(define-method (c-type-name (type <chop-block-key-type>))
  "chop_block_key_t")

(define-method (pre-call-arg-cg (type <chop-block-key-type>)
				(param <gw-value>) error-var)
  (let ((handle-var (string-append (var param) "_handle"))
	(content-var (string-append (var param) "_buf"))
	(size-var (string-append (var param) "_size"))
	(increment-var (string-append (var param) "_inc")))
    ;; Declare the variables that will hold the necessary information
    (list (format #f "\n/* pre-call-arg-cg ~a */\n" type)
	  "scm_t_array_handle " handle-var "; "
	  "const scm_t_uint8 *" content-var "; "
	  "size_t " size-var " = 0; "
	  "ssize_t " increment-var " = 0;\n\n"

	  ;; call (indirectly) `unwrap-value-cg'.
	  (next-method))))

(define-method (unwrap-value-cg (type <chop-block-key-type>)
				(value <gw-value>) error-var)
  ;; This method is actually called by `pre-call-arg-cg'.
  (let ((handle-var (string-append (var value) "_handle"))
	(size-var (string-append (var value) "_size"))
	(increment-var (string-append (var value) "_inc"))
	(content-var (string-append (var value) "_buf")))
    (list "if (SCM_FALSEP (scm_u8vector_p (" (scm-var value) ")))"
	  `(gw:error ,error-var type ,(wrapped-var value))
	  "else { "
	  content-var " = " "scm_u8vector_elements ("
	  (scm-var value)
	  ", &" handle-var ", &" size-var ", &" increment-var ");\n"
	  "chop_block_key_init (&" (var value) ", (char *)" content-var
	  ", " size-var ", NULL, NULL);\n"
	  "}\n")))

(define-method (wrap-value-cg (type <chop-block-key-type>)
			      (value <gw-value>) error-var)
  (let ((key-buf (string-append (var value) "_buf")))
    (format #t "wrap-value-cg/block-key~%")
    (list "{ /* wrap-value-cg/block-key */\n"
	  "  scm_t_uint8 *" key-buf ";\n"
	  "  "key-buf" = (scm_t_uint8 *)"
	  "scm_malloc (chop_block_key_size (&"(var value)"));\n"
	  "  memcpy ("key-buf", chop_block_key_buffer (&"(var value)"),\n"
	  "          chop_block_key_size (&"(var value)"));\n"
	  "  "(scm-var value)" = scm_take_u8vector ("key-buf", "
	  "chop_block_key_size (&"(var value)"));\n"
	  "  chop_block_key_free (&"(var value)");\n"
	  "}\n")))

(define-method (post-call-arg-cg (type <chop-block-key-type>)
				 (param <gw-value>) error-var)
  (if-typespec-option
   param 'out

   ;; `out' param:  call `wrap-value-cg'
   (next-method)

   ;; `in' param:  release the previously acquired handle
   (let ((handle-var (string-append (var param) "_handle")))
     (list "\n/* post-call-arg-cg/block-key */\n"
	   "if (scm_u8vector_p (" (scm-var param) ") == SCM_BOOL_T)\n{\n"
	   "scm_array_handle_release (&" handle-var ");\n"
	   "scm_remember_upto_here (" (scm-var param) ");\n}\n"))))

(define-method (call-arg-cg (type <chop-block-key-type>)
			    (value <gw-value>))
  ;; Pass a pointer to the key object rather than the key itself.
  (list "& /* key! */" (var value)))



;;;
;;; Include files.
;;;

(define-method (global-declarations-cg (ws <chop-store-wrapset>))
  (list (next-method)
	"#include <chop/chop.h>\n#include <chop/stores.h>\n\n"
	"/* The following are needed for the MODE arg of `gdbm-store-open'.  */\n"
	"#include <sys/types.h>\n"
	"#include <sys/stat.h>\n"
	"#include <fcntl.h>\n\n"
	"#include \"core-support.h\"\n"
	"#include \"stores-support.c\"\n\n"))



;;;
;;; Wrapping.
;;;

(define-method (initialize (ws <chop-store-wrapset>) initargs)
  (format #t "initializing ~a~%" ws)

  (slot-set! ws 'shlib-path "libguile-chop")

  (next-method ws (append '(#:module (chop stores)) initargs))

  ;; error codes

  (wrap-constant! ws
		  #:name 'store-error/generic
		  #:type 'long
		  #:value "CHOP_STORE_ERROR"
		  #:description "Generic store error")

  (wrap-constant! ws
		  #:name 'store-error/block-unavailable
		  #:type 'long
		  #:value "CHOP_STORE_BLOCK_UNAVAIL"
		  #:description "Block unavailable")

  (wrap-constant! ws
		  #:name 'store/end
		  #:type 'long
		  #:value "CHOP_STORE_END"
		  #:description "End of block store")

  ;; types

  (add-type! ws (make <chop-block-key-type>
		  #:name '<block-key>))

  (wrap-as-chop-object! ws
			#:name '<store>
			#:c-type-name "chop_block_store_t *"
			#:c-const-type-name "const chop_block_store_t *")

  (wrap-as-chop-object! ws
			#:name '<block-iterator>
			#:c-type-name "chop_block_iterator_t *"
			#:c-const-type-name "const chop_block_iterator_t *")

  ;; constructors

  (wrap-function! ws
		  #:name 'dummy-block-store-open
		  #:c-name "chop_dummy_block_store_open_alloc"
		  #:returns '<store>
		  #:arguments '(((mchars caller-owned) name)))

  (wrap-function! ws
		  #:name 'dummy-proxy-block-store-open
		  #:c-name "chop_dummy_proxy_block_store_open_alloc"
		  #:returns '<store>
		  #:arguments '(((mchars caller-owned) name)
				((<store> aggregated) backend)))

  (wrap-function! ws
		  #:name 'smart-block-store-open
		  #:c-name "chop_smart_block_store_open_alloc"
		  #:returns '<store>
		  #:arguments '(((<store> aggregated) backend)))

  (wrap-function! ws
		  #:name 'file-based-block-store-open
		  #:c-name "chop_file_based_store_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((mchars caller-owned) class-name)
				((mchars caller-owned) file-name)
				(int open-flags (default "O_RDWR | O_CREAT"))
				(int mode (default "S_IRUSR | S_IWUSR"))
				((<store> out) new-store)))

  ;; XXX: In the end, the `gdbm', `tdb', etc. functions below should be
  ;; superseded by `file-based-block-store-open'.  This would allow Scheme
  ;; code to use libchop even if not all those store classes are available.

  (wrap-function! ws
		  #:name 'gdbm-block-store-open
		  #:c-name "chop_gdbm_block_store_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((mchars caller-owned) name)
				(int block-size (default 0))
				(int open-flags (default "O_RDWR | O_CREAT"))
				(int mode (default "S_IRUSR | S_IWUSR"))
				((<store> out) new-gdbm-store)))

  (wrap-function! ws
		  #:name 'tdb-block-store-open
		  #:c-name "chop_tdb_block_store_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((mchars caller-owned) name)
				(int hash-size (default 0))
				(int open-flags (default "O_RDWR | O_CREAT"))
				(int mode (default "S_IRUSR | S_IWUSR"))
				((<store> out) new-tdb-store)))

  (wrap-function! ws
		  #:name 'sunrpc-block-store-open
		  #:c-name "chop_sunrpc_block_store_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((mchars caller-owned) host)
				(unsigned-int port)
				((mchars caller-owned) protocol)
				((<store> out) new-store)))

  (wrap-function! ws
                  #:name 'sunrpc/tls-block-store-simple-open
                  #:c-name "chop_sunrpc_tls_block_store_simple_open_alloc"
                  #:returns '<errcode>
                  #:arguments '(((mchars caller-owned) host)
                                (unsigned-int port)
                                ((mchars caller-owned) pubkey-file)
                                ((mchars caller-owned) privkey-file)
                                ((<store> out) new-store)))

  (wrap-function! ws
		  #:name 'filtered-store-open
		  #:c-name "chop_filtered_store_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((<filter> aggregated) input-filter)
				((<filter> aggregated) output-filter)
				((<store> aggregated) backend)
				(bool close-backend? (default #f))
				((<store> out) store)))

  (wrap-function! ws
		  #:name 'make-block-store
		  #:c-name "chop_make_scheme_block_store"
		  #:returns 'scm
		  #:arguments '((scm read-block-proc)
				(scm write-block-proc)
				(scm block-exists-proc)
				(scm remove-block-proc)
				(scm first-block-proc)
				(scm iterator-next!-proc)
				(scm sync-proc)
				(scm close-proc)))


  ;; methods

  (wrap-function! ws
		  #:name 'store-write-block
		  #:returns '<errcode>
		  #:c-name "chop_store_write_block"
		  #:arguments '(((<store> caller-owned) store)
				(<block-key> key)
				(<input-buffer> buffer))
		  #:description "Read from @var{stream}.")

  (wrap-function! ws
		  #:name 'store-read-block
		  #:returns '<errcode>
		  #:c-name "chop_store_read_block_alloc_u8vector"
		  #:arguments '((<store> store)
				(<block-key> key)
				((scm out) buffer))
		  #:description "Read from @var{store} the block whose key
is @var{key} and return a u8vector representing its content.")

  (wrap-function! ws
		  #:name 'store-block-exists?
		  #:returns '<errcode>
		  #:c-name "chop_store_block_exists"
		  #:arguments '((<store> store)
				(<block-key> key)
				((bool out) exists?))
		  #:description "Return @code{#t} if block under key
@var{key} exists in @var{store}.")

  (wrap-function! ws
		  #:name 'store-delete-block
		  #:returns '<errcode>
		  #:c-name "chop_store_delete_block"
		  #:arguments '((<store> store)
				(<block-key> key))
		  #:description "Delete from @var{store} block with key
@var{key}.")

  (wrap-function! ws
		  #:name 'store-first-block
		  #:returns '<errcode>
		  #:c-name "chop_store_first_block_alloc"
		  #:arguments '(((<store> aggregated) store)
				((<block-iterator> out) it)) #:description
"Return a block iterator to the first block available in @var{store}.")

  (wrap-function! ws
		  #:name 'store-sync
		  #:returns '<errcode>
		  #:c-name "chop_store_sync"
		  #:arguments '((<store> store)))

  (wrap-function! ws
		  #:name 'store-close
		  #:returns '<errcode>
		  #:c-name "chop_store_close"
		  #:arguments '((<store> store)))


  ;; block iterator methods

  (wrap-function! ws
		  #:name 'block-iterator-nil?
		  #:returns 'bool
		  #:c-name "chop_block_iterator_is_nil"
		  #:arguments '((<block-iterator> it))
		  #:description "Return true of block iterator @var{it} is
nil.")

  (wrap-function! ws
		  #:name 'block-iterator-key
		  #:returns '<errcode>
		  #:c-name "chop_block_iterator_key_check_non_nil"
		  #:arguments '((<block-iterator> it)
				((<block-key> out) key))
		  #:description "Return the key corresponding to iterator
@var{it}, provided @var{it} is not nil.")

  (wrap-function! ws
		 #:name 'block-iterator-next!
		 #:returns '<errcode>
		 #:c-name "chop_block_iterator_next"
		 #:arguments '((<block-iterator> it))
		 #:description "Update block iterator @var{it} so that it
points to the next block available in the underlying store.  If not other
block is available, then an exception is raised with value @code{store/end}.")


)

;; Local Variables:
;; mode: scheme
;; scheme-program-name: "guile"
;; End: