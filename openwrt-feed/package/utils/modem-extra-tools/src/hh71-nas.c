/* SPDX-License-Identifier: Apache-2.0 */
/* Minimal ARM EABI QMI client over the stock MSM IPC router.
 * DMS supplies device LTE capabilities; NAS reads/sets LTE preference. */
#include <stdint.h>
#include <stddef.h>

void *memset(void *p,int c,size_t n) { unsigned char *v=p; while(n--) *v++=(unsigned char)c; return p; }
void *memcpy(void *p,const void *s,size_t n) { unsigned char *v=p; const unsigned char *u=s; while(n--) *v++=*u++; return p; }
static long sc3(long id,long a,long b,long c) { register long r0 __asm__("r0")=a,r1 __asm__("r1")=b,r2 __asm__("r2")=c,r7 __asm__("r7")=id; __asm__ volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory","cc"); return r0; }
static long sc6(long id,long a,long b,long c,long d,long e,long f) { register long r0 __asm__("r0")=a,r1 __asm__("r1")=b,r2 __asm__("r2")=c,r3 __asm__("r3")=d,r4 __asm__("r4")=e,r5 __asm__("r5")=f,r7 __asm__("r7")=id; __asm__ volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5),"r"(r7):"memory","cc"); return r0; }
static size_t length(const char *s) { size_t n=0; while(s[n]) n++; return n; }
static int equal(const char *a,const char *b) { while(*a&&*a==*b){a++;b++;} return *a==*b; }
static int fail(const char *m) { sc3(4,2,(long)m,length(m)); sc3(4,2,(long)"\n",1); return 1; }
static unsigned u16(const unsigned char *v) { return v[0]|((unsigned)v[1]<<8); }
static int nibble(char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; return -1; }
static int parse_mask(const char *s,unsigned char *out)
{
    unsigned i,any=0,anchor=0;
    if(length(s)!=16) return -1;
    for(i=0;i<8;i++) {
        int a=nibble(s[2*i]),b=nibble(s[2*i+1]);
        if(a<0||b<0) return -1;
        out[i]=(unsigned char)((a<<4)|b); any|=out[i];
        anchor|=out[i]&(i==3?0x7f:0xff); /* B32 is SDL, not an anchor. */
    }
    return any&&anchor?0:-1;
}
static int subset(const unsigned char *mask,const unsigned char *cap)
{ unsigned i; for(i=0;i<8;i++) if(mask[i]&~cap[i]) return 0; return 1; }
static int same(const unsigned char *left,const unsigned char *right)
{ unsigned i; for(i=0;i<8;i++) if(left[i]!=right[i]) return 0; return 1; }
static void merge(unsigned char *out,const unsigned char *left,const unsigned char *right)
{ unsigned i; for(i=0;i<8;i++) out[i]=left[i]|right[i]; }
static void print_mask(const unsigned char *v)
{
    static const char d[]="0123456789abcdef"; char text[17]; unsigned i;
    for(i=0;i<8;i++){text[2*i]=d[v[i]>>4];text[2*i+1]=d[v[i]&15];}
    text[16]='\n'; sc3(4,1,(long)text,sizeof(text));
}

struct endpoint { uint32_t node,port,service,instance; };
struct lookup { uint32_t service,instance; int capacity,found; uint32_t mask; struct endpoint entries[8]; };
struct address { uint16_t family,pad; uint8_t type,pad2[3]; uint32_t node,port; uint8_t reserved,pad3[3]; };
struct pollfd_local { int fd; short events,revents; };
_Static_assert(sizeof(struct address)==20,"unexpected MSM IPC ABI");

static int open_service(uint32_t service,struct address *addr)
{
    struct lookup q={0,1,8,0,0xffffffff,{{0}}}; long fd;
    q.service=service; fd=sc3(281,27,2,0);
    if(fd<0) return -1;
    if(sc3(54,fd,0xc014c302,(long)&q)<0||q.found!=1||q.entries[0].service!=service||q.entries[0].instance!=1) { sc3(6,fd,0,0); return -1; }
    memset(addr,0,sizeof(*addr)); addr->family=27; addr->type=2; addr->node=q.entries[0].node; addr->port=q.entries[0].port;
    return (int)fd;
}
static int request_mask(int fd,struct address *addr,uint16_t message_id,int setting,unsigned char tlv,unsigned char *mask)
{
    unsigned char msg[22]={0,1,0,0,0,0,0},reply[2048]; struct pollfd_local pfd={fd,1,0};
    unsigned out=7,attempt; msg[3]=(unsigned char)message_id; msg[4]=(unsigned char)(message_id>>8);
    if(setting) {
        msg[5]=15; out=22; msg[7]=tlv; msg[8]=8; memcpy(msg+10,mask,8);
        msg[18]=0x17; msg[19]=1; msg[21]=1; /* permanent */
    }
    if(sc6(290,fd,(long)msg,out,0,(long)addr,sizeof(*addr))!=(long)out) return -1;
    for(attempt=0;attempt<30;attempt++) {
        long ready=sc3(168,(long)&pfd,1,250),got; unsigned off,total; int result=-1,found=0;
        if(ready<0) return -1;
        if(!ready) continue;
        got=sc6(292,fd,(long)reply,sizeof(reply),0x40,0,0); if(got<7)continue;
        if(reply[0]!=2||u16(reply+1)!=1||u16(reply+3)!=message_id)continue;
        total=u16(reply+5)+7; if(total!=(unsigned)got)return -1;
        for(off=7;off<total;) {
            unsigned size,type; if(total-off<3)return -1;
            type=reply[off];size=u16(reply+off+1);off+=3;if(size>total-off)return -1;
            if(type==2){if(size!=4)return -1;result=(u16(reply+off)==0&&u16(reply+off+2)==0)?0:-1;}
            if(!setting&&type==tlv){if(size!=8||found)return -1;memcpy(mask,reply+off,8);found=1;}
            off+=size;
        }
        return result==0&&(setting||found)?0:-1;
    }
    return -1;
}
static int capabilities(unsigned char *mask)
{
    struct address a; int fd=open_service(2,&a),result; /* QMI DMS */
    if(fd<0) return -1;
    result=request_mask(fd,&a,0x45,0,0x10,mask);
    sc3(6,fd,0,0);
    return result;
}
static int nas(int setting,unsigned char *mask)
{
    struct address a; int fd=open_service(3,&a),result; /* QMI NAS */
    if(fd<0) return -1;
    result=request_mask(fd,&a,setting?0x33:0x34,setting,0x15,mask);
    sc3(6,fd,0,0);
    return result;
}
__attribute__((used)) static int main_c(int argc,char **argv)
{
    unsigned char mask[8]={0},cap[8]={0},expected[8]={0},current[8]={0},available[8]={0};
    int setting,compatible,restoring;
    if(argc<2||(!equal(argv[1],"get")&&!equal(argv[1],"set")&&!equal(argv[1],"apply")&&
        !equal(argv[1],"restore")&&!equal(argv[1],"capabilities")))
        return fail("usage: hh71-nas get | capabilities | set MASK | apply MASK EXPECTED | restore MASK EXPECTED");
    if(equal(argv[1],"capabilities")) {
        if(argc!=2||capabilities(mask))return fail("QMI DMS LTE capabilities unavailable");
        print_mask(mask);return 0;
    }
    setting=equal(argv[1],"set"); compatible=equal(argv[1],"apply"); restoring=equal(argv[1],"restore");
    if((!setting&&!compatible&&!restoring&&argc!=2)||
       (setting&&(argc!=3||parse_mask(argv[2],mask)))||
       ((compatible||restoring)&&(argc!=4||parse_mask(argv[2],mask)||parse_mask(argv[3],expected))))
        return fail("invalid LTE preference mask");
    if(setting) {
        if(capabilities(cap))return fail("QMI DMS LTE capabilities unavailable");
        if(!subset(mask,cap))return fail("LTE preference exceeds modem capabilities");
    }
    if(compatible||restoring) {
        if(nas(0,current))return fail("QMI NAS current preference unavailable");
        if(!same(current,expected))return fail("LTE preference changed concurrently; no write performed");
        if(compatible) {
            if(capabilities(cap))return fail("QMI DMS LTE capabilities unavailable");
            merge(available,cap,current);
            if(!subset(mask,available))return fail("LTE preference contains a new unreported band");
        }
    }
    if(nas(setting||compatible||restoring,mask))return fail("QMI NAS request failed, timed out or returned an invalid reply");
    if(!setting&&!compatible&&!restoring) print_mask(mask);
    return 0;
}
__attribute__((naked,noreturn)) void _start(void)
{ __asm__ volatile("ldr r0,[sp]\nadd r1,sp,#4\nbl main_c\nmov r7,#1\nsvc 0\n":::"memory"); __builtin_unreachable(); }
