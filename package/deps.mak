#
# This file has been generated by tools/gen-deps.sh
#

src/socklog/socklog.o src/socklog/socklog.lo: src/socklog/socklog.c
src/socklog/uncat.o src/socklog/uncat.lo: src/socklog/uncat.c

socklog: EXTRA_LIBS :=
socklog: src/socklog/socklog.o -lskarnet
uncat: EXTRA_LIBS :=
uncat: src/socklog/uncat.o -lskarnet
