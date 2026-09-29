/* SPDX-License-Identifier: Apache-2.0 */
/* Read-only QMI UIM personalization status for the inserted SIM (slot 0). */
#include <stddef.h>
#include <stdint.h>

void *memset(void *p, int c, size_t n) {
    unsigned char *v = p;
    while (n--) *v++ = (unsigned char)c;
    return p;
}
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}
static long sc3(long id, long a, long b, long c) {
    register long r0 __asm__("r0") = a, r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c, r7 __asm__("r7") = id;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return r0;
}
static long sc6(long id, long a, long b, long c, long d, long e, long f) {
    register long r0 __asm__("r0") = a, r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c, r3 __asm__("r3") = d;
    register long r4 __asm__("r4") = e, r5 __asm__("r5") = f;
    register long r7 __asm__("r7") = id;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7) : "memory", "cc");
    return r0;
}
static void out(const char *text) {
    size_t n = 0;
    while (text[n]) n++;
    sc3(4, 1, (long)text, n);
}
static int fail(const char *text) {
    out(text);
    return 1;
}
static void hex(unsigned value) {
    static const char digits[] = "0123456789abcdef";
    char text[3] = {digits[(value >> 4) & 15], digits[value & 15], 0};
    out(text);
}
static unsigned u16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }

struct endpoint { uint32_t node, port, service, instance; };
struct lookup { uint32_t service, instance; int capacity, found; uint32_t mask; struct endpoint entries[8]; };
struct address {
    uint16_t family, pad;
    uint8_t type, pad2[3];
    uint32_t node, port;
    uint8_t reserved, pad3[3];
};
struct pollfd_local { int fd; short events, revents; };
_Static_assert(sizeof(struct address) == 20, "unexpected MSM IPC ABI");

__attribute__((used)) static int run(void) {
    struct lookup q = {11, 1, 8, 0, 0xffffffff, {{0}}};
    struct address addr = {0};
    struct pollfd_local pfd;
    unsigned char request[] = {0, 1, 0, 0x3a, 0, 7, 0, 0x10, 4, 0, 2, 0, 0, 0};
    unsigned char reply[512];
    long fd = sc3(281, 27, 2, 0);
    if (fd < 0) return fail("UIM socket unavailable\n");
    if (sc3(54, fd, 0xc014c302, (long)&q) < 0 || q.found != 1 ||
        q.entries[0].service != 11 || q.entries[0].instance != 1) {
        sc3(6, fd, 0, 0);
        return fail("UIM service unavailable\n");
    }
    addr.family = 27;
    addr.type = 2;
    addr.node = q.entries[0].node;
    addr.port = q.entries[0].port;
    if (sc6(290, fd, (long)request, sizeof(request), 0, (long)&addr, sizeof(addr)) !=
        (long)sizeof(request)) {
        sc3(6, fd, 0, 0);
        return fail("UIM request failed\n");
    }
    pfd.fd = (int)fd;
    pfd.events = 1;
    pfd.revents = 0;
    for (unsigned attempt = 0; attempt < 20; attempt++) {
        if (sc3(168, (long)&pfd, 1, 250) <= 0) continue;
        long n = sc6(292, fd, (long)reply, sizeof(reply), 0x40, 0, 0);
        if (n < 7 || reply[0] != 2 || u16(reply + 1) != 1 || u16(reply + 3) != 0x3a)
            continue;
        unsigned end = 7 + u16(reply + 5);
        if (end != (unsigned)n) break;
        const unsigned char *features = NULL;
        unsigned count = 0;
        int result_seen = 0, result_ok = 0, valid = 1;
        for (unsigned pos = 7; pos < end;) {
            if (end - pos < 3) { valid = 0; break; }
            unsigned type = reply[pos], length = u16(reply + pos + 1);
            pos += 3;
            if (length > end - pos) { valid = 0; break; }
            if (type == 2) {
                if (length != 4 || result_seen) { valid = 0; break; }
                result_seen = 1;
                result_ok = u16(reply + pos) == 0 && u16(reply + pos + 2) == 0;
            } else if (type == 0x11) {
                if (features || length < 1 || length != 1U + 3U * reply[pos]) {
                    valid = 0; break;
                }
                features = reply + pos;
                count = reply[pos];
            }
            pos += length;
        }
        sc3(6, fd, 0, 0);
        if (!valid || !result_seen || !result_ok || !features)
            return fail("UIM status incomplete\n");
        out("slot=00 feature_count="); hex(count); out("\n");
        for (unsigned i = 0; i < count; i++) {
            out("slot=00 feature="); hex(features[1 + 3*i]);
            out(" verify="); hex(features[2 + 3*i]);
            out(" unblock="); hex(features[3 + 3*i]); out("\n");
        }
        return 0;
    }
    sc3(6, fd, 0, 0);
    return fail("UIM status unavailable\n");
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile("bl run\nmov r7,#1\nsvc 0" ::: "memory");
    __builtin_unreachable();
}
