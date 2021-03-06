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

(define-module (block-indexers-spec)
  #:use-module (core-spec)
  #:use-module (hash-spec)
  #:use-module (cipher-spec)
  #:use-module (stores-spec)
  #:use-module (objects-spec)
  #:use-module (logs-spec)

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

  #:export (<chop-block-indexer-wrapset>))


;; the wrapset itself.

(define-class <chop-block-indexer-wrapset> (<gw-guile-wrapset>)
  #:id 'block-indexers
  #:dependencies '(standard core objects hash cipher stores logs))


(define-method (global-declarations-cg (ws <chop-block-indexer-wrapset>))
  (list (next-method)
	"#include <chop/chop.h>\n#include <chop/block-indexers.h>\n"
	"#include \"core-support.h\"\n"
	"#include \"block-indexers-support.c\"\n\n"))




(define-method (initialize (ws <chop-block-indexer-wrapset>) initargs)
  (format #t "initializing ~a~%" ws)

  (slot-set! ws 'shlib-path "libguile-chop")

  (next-method ws (append '(#:module (chop block-indexers)) initargs))

  (wrap-constant! ws
		  #:name 'block-indexer-error/generic
		  #:type 'long
		  #:value "CHOP_BLOCK_INDEXER_ERROR"
		  #:description "Generic block indexer error")

  (wrap-constant! ws
		  #:name 'block-fetcher-error/generic
		  #:type 'long
		  #:value "CHOP_BLOCK_FETCHER_ERROR"
		  #:description "Generic block fetcher error")

  (wrap-as-chop-object! ws
			#:name '<block-indexer>
			#:c-type-name "chop_block_indexer_t *"
			#:c-const-type-name "const chop_block_indexer_t *")

  (wrap-as-chop-object! ws
			#:name '<block-fetcher>
			#:c-type-name "chop_block_fetcher_t *"
			#:c-const-type-name "const chop_block_fetcher_t *")

  (wrap-as-chop-object! ws
			#:name '<index-handle>
			#:c-type-name "chop_index_handle_t *"
			#:c-const-type-name "const chop_index_handle_t *")

  ;; constructors

  (wrap-function! ws
		  #:name 'hash-block-indexer-open
		  #:c-name "chop_hash_block_indexer_open_alloc"
		  #:returns '<errcode>
		  #:arguments '((hash-method content-hash-method)
				((<block-indexer> out) bi)))

  (wrap-function! ws
		  #:name 'chk-block-indexer-open
		  #:c-name "chop_chk_block_indexer_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((<cipher-handle> aggregated) cipher)
				(hash-method key-hash-method)
				(hash-method block-id-hash-method)
				((<block-indexer> out) bi)))

  (wrap-function! ws
		  #:name 'uuid-block-indexer-open
		  #:c-name "chop_uuid_block_indexer_open_alloc"
		  #:returns '<errcode>
		  #:arguments '(((<block-indexer> out) bi)))

  (wrap-function! ws
		  #:name 'integer-block-indexer-open
		  #:c-name "chop_integer_block_indexer_open_alloc"
		  #:returns '<errcode>
		  #:arguments '((unsigned-long         start (default 0))
                                ((<block-indexer> out) bi)))

  ;; methods

  (wrap-function! ws
		  #:name 'block-indexer-index
		  #:c-name "chop_block_indexer_index_alloc"
		  #:returns '<errcode>
		  #:arguments '((<block-indexer> bi)
				(<store> store)
				(<input-buffer> data)
				((<index-handle> out) handle)))

  (wrap-function! ws
		  #:name 'block-indexer-fetch
		  #:c-name "chop_block_fetcher_fetch_alloc_u8vector"
		  #:returns '<errcode>
		  #:arguments '((<block-fetcher> bf)
				(<index-handle> index)
				(<store> store)
				((scm out) buffer)))

  (wrap-function! ws
		  #:name 'block-indexer-make-fetcher
		  #:c-name "chop_block_indexer_make_fetcher_alloc"
		  #:returns '<errcode>
		  #:arguments '((<block-indexer> bi)
				((<block-fetcher> out) bf)))


  ;; index handles

  (wrap-function! ws
		  #:name 'index-handle-ascii-serialize
		  ;; FIXME:  There is currently a leak here:  the returned
		  ;; string is never freed.  That's a G-Wrap problem.
		  #:returns '<errcode>
		  #:c-name "chop_index_handle_ascii_serialize"
		  #:arguments '((<index-handle> handle)
				((mchars out caller-owned) serialized)))

  (wrap-function! ws
		  #:name 'index-handle-ascii-deserialize
		  #:returns '<errcode>
		  #:c-name "chop_index_handle_ascii_deserialize"
		  #:arguments '((<chop-class> index-class)
				((mchars caller-owned) ascii-handle)
				((<index-handle> out) handle)))

)

;; Local Variables:
;; mode: scheme
;; scheme-program-name: "guile"
;; End:

;; arch-tag: 8db4c9fd-cc7e-4456-a126-c8e3ece2b4b6
