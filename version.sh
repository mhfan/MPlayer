#!/bin/sh

test "$1" && extra="-$1"

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)

if test -z $version ; then
# Extract revision number from file used by daily tarball snapshots
# or from the places different Subversion versions have it.
svn_revision=$(cat snapshot_version 2> /dev/null)
test $svn_revision || svn_revision=$(LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2)
test $svn_revision || svn_revision=$(grep revision .svn/entries 2>/dev/null | cut -d '"' -f2)
test $svn_revision || svn_revision=$(sed -n -e '/^dir$/{n;p;q;}' .svn/entries 2>/dev/null)

test $svn_revision && svn_revision=-r$svn_revision
[ -d .hg  ] && svn_revision=$svn_revision-h$(LC_ALL=C hg parents 2> /dev/null | \grep '^changeset:' | cut -d':' -f3)
[ -d .git ] && svn_revision=$svn_revision-g$(git log -1 --pretty=format:%h)

test $svn_revision || svn_revision=-UNKNOWN
version=$svn_revision
fi

NEW_REVISION="#define VERSION \"dev${version}${extra}\""	# mhfan
OLD_REVISION=$(head -n 1 version.h 2> /dev/null)
TITLE='#define MP_TITLE "%s " VERSION " (C) 2000-2011 MPlayer Team"'
TITLE="$TITLE\"\n\thacked by mhfan and compiled by $USER at [\" __TIME__ \", \" __DATE__ \"]\n\""

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$TITLE
EOF
fi

