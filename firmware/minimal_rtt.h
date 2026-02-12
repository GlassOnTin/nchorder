// Minimal RTT interface - no SDK dependencies
#ifndef MINIMAL_RTT_H
#define MINIMAL_RTT_H

void rtt_init(void);
void rtt_print(const char* str);
void rtt_printf(const char* fmt, ...);

#endif
