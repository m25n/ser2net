#ifndef __SERIAL_LIB_H__
#define __SERIAL_LIB_H__

int serial_init(const char* port, int baud);
int serial_flush(int fd);
int serial_close(int fd);

#endif
