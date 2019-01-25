#run inside the site's (i.e. 'default') directory
find ./ | cpio -ov -H newc > dir.cpio
xxd -i dir.cpio  > ../../dir.h
