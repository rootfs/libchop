#!/bin/sh
#
# Write a list of the built-in classes' names, looking at the given source
# files, in a way that is suitable as a `gperf' input.

echo '%{'
echo '#include <chop/chop.h>'
echo '#include <chop/serializable.h>'
echo '#include <chop/indexers.h>'
echo '#include <chop/filters.h>'
echo '%}'
echo
echo 'struct chop_class_entry { const char *name; const void *class; };'
echo '%%'

cat $@ | \
grep '^CHOP_DEFINE_RT_CLASS' | \
sed -es'/^CHOP_DEFINE_RT_CLASS\(_WITH_METACLASS\)\? *(\([a-zA-Z_]\+\),.*$/\2, \&chop_\2_class/g'