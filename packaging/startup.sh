#!/var/jb/bin/sh
set -eu
umask 077
/var/jb/bin/sleep 5
while IFS= read -r hash; do
    [ "${#hash}" = 40 ] || exit 1
    case "$hash" in *[!0-9a-f]*) exit 1 ;; esac
    /var/jb/basebin/jbctl trustcache add "$hash"
done < /var/jb/usr/libexec/basebanddisabler/trustcache.list
exec /var/jb/usr/libexec/basebanddisabler/basebandctl boot
