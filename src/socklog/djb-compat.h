#ifndef DJB_COMPAT_H
#define DJB_COMPAT_H

#define wait_crashed(w) ((w) & 127)
#define wait_exitcode(w) ((w) >> 8)
#define wait_stopsig(w) ((w) >> 8)
#define wait_stopped(w) (((w) & 127) == 127)

#define buffer_peek(s) ((s)->c.x + (s)->c.n)
#define buffer_seek(s,len) \
  (s)->c.n += len; \
  (s)->c.p -= len;

#endif

#define buffer_feed(b) buffer_fill(b)
