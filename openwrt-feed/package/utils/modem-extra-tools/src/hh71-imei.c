/* SPDX-License-Identifier: Apache-2.0 */
/* Narrow ARM EABI Qualcomm DCI client for NV item 550 (UE IMEI) only.
 * It accepts a valid 15-digit IMEI, preserves unused bytes and verifies readback. */
#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_IOCTL 54
#define SYS_POLL 168
#define O_RDWR 2
#define O_NONBLOCK 04000
#define POLLIN 1
#define DCI_DEINIT 21
#define DCI_REG 23
#define DCI_TYPE 0x40
#define DCI_RESPONSE 0
#define DCI_OK 1001
#define NV_READ 0x26
#define NV_WRITE 0x27
#define NV_ITEM 550
#define NV_LEN 128
#define BUFFER_LEN 16384

struct pollfd_local { int fd; short events,revents; };
struct diag_dci_reg { int client_id; uint16_t notifications; int signal_type; int token; } __attribute__((packed));

#ifdef HOST_TEST
static long sc1(long n,long a) { (void)n;(void)a;return -1; }
static long sc3(long n,long a,long b,long c) { (void)n;(void)a;(void)b;(void)c;return -1; }
#else
static long sc1(long n,long a) { register long r0 __asm__("r0")=a,r7 __asm__("r7")=n; __asm__ volatile("svc 0":"+r"(r0):"r"(r7):"memory","cc"); return r0; }
static long sc3(long n,long a,long b,long c) { register long r0 __asm__("r0")=a,r1 __asm__("r1")=b,r2 __asm__("r2")=c,r7 __asm__("r7")=n; __asm__ volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory","cc"); return r0; }
#endif
void *memset(void *p,int c,size_t n) { uint8_t *v=p;while(n--)*v++=(uint8_t)c;return p; }
static size_t length(const char *s) { size_t n=0; while(s[n])n++; return n; }
static int equal(const char *a,const char *b) { while(*a&&*a==*b){a++;b++;}return *a==*b; }
static void copy(void *d,const void *s,size_t n) { uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++; }
static int same(const void *a,const void *b,size_t n) { const uint8_t *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1; }
static void write_all(int fd,const void *data,size_t n) { const uint8_t *p=data;while(n){long w=sc3(SYS_WRITE,fd,(long)p,n);if(w<=0)return;p+=w;n-=(size_t)w;} }
static int fail(const char *m) { write_all(2,"hh71-imei: ",11);write_all(2,m,length(m));write_all(2,"\n",1);return 1; }
static int register_dci(int fd) { struct diag_dci_reg r={0,0,0,0};return (int)sc3(SYS_IOCTL,fd,DCI_REG,(long)&r); }

static int send_packet(int fd,int client,const uint8_t *packet,size_t n)
{
    uint8_t request[12+3+NV_LEN];int32_t v=DCI_TYPE;long wrote;
    copy(request,&v,4);v=1;copy(request+4,&v,4);v=client;copy(request+8,&v,4);copy(request+12,packet,n);
    wrote=sc3(SYS_WRITE,fd,(long)request,12+n);return wrote==DCI_OK?0:-1;
}
static int find_response(const uint8_t *b,size_t n,uint8_t command,uint8_t *out,size_t *out_n)
{
    size_t off=12;int32_t v,data_n;
    if(n<off) return 0;
    copy(&v,b,4);
    if(v!=DCI_TYPE) return 0;
    copy(&data_n,b+8,4);
    if(data_n<13||(size_t)data_n>n-12) return 0;
    copy(&v,b+off,4);
    if(v!=DCI_RESPONSE) return 0;
    off+=4;
    copy(&data_n,b+off,4);if(data_n<4||off+5+(size_t)data_n>n)return 0;off+=5;
    copy(&v,b+off,4);if(v!=1)return 0;off+=4;data_n-=4;
    if(data_n<3||(size_t)data_n>3+NV_LEN+2||b[off]!=command||b[off+1]!=(NV_ITEM&255)||b[off+2]!=(NV_ITEM>>8))return 0;
    copy(out,b+off,(size_t)data_n);*out_n=(size_t)data_n;return 1;
}
static int transact(int fd,int client,const uint8_t *request,size_t request_n,uint8_t *response,size_t *response_n)
{
    struct pollfd_local pfd={fd,POLLIN,0};uint8_t buffer[BUFFER_LEN];int attempt;
    if(send_packet(fd,client,request,request_n)<0)return -1;
    for(attempt=0;attempt<20;attempt++) {
        long ready=sc3(SYS_POLL,(long)&pfd,1,250),got;if(ready<0)return -1;if(!ready)continue;
        got=sc3(SYS_READ,fd,(long)buffer,sizeof(buffer));if(got<0)continue;
        if(find_response(buffer,(size_t)got,request[0],response,response_n))return 0;
    }
    return -1;
}
static int nv_request(int fd,int client,int writing,const uint8_t *value,uint8_t *out)
{
    uint8_t request[3+NV_LEN],response[3+NV_LEN+2];size_t request_n=3,response_n=0;
    request[0]=writing?NV_WRITE:NV_READ;request[1]=NV_ITEM&255;request[2]=NV_ITEM>>8;
    if(writing){copy(request+3,value,NV_LEN);request_n+=NV_LEN;}
    if(transact(fd,client,request,request_n,response,&response_n)<0)return -1;
    if(response_n!=3+NV_LEN+2||response[3+NV_LEN]!=0||response[3+NV_LEN+1]!=0)return -1;
    if(writing&&!same(response+3,value,NV_LEN)) return -1;
    copy(out,response+3,NV_LEN);
    return 0;
}
static int valid_imei(const char *s)
{
    unsigned i,sum=0,nonzero=0;if(length(s)!=15)return 0;
    for(i=0;i<15;i++) {
        unsigned d;if(s[i]<'0'||s[i]>'9')return 0;d=(unsigned)(s[i]-'0');nonzero|=d;
        if(i<14&&(i&1)){d*=2;sum+=d>=10?d-9:d;}else sum+=d;
    }
    while(sum>=10) sum-=10;
    return nonzero&&sum==0;
}
static void encode(const char *s,uint8_t *raw)
{
    unsigned i;raw[0]=8;raw[1]=(uint8_t)(((s[0]-'0')<<4)|0x0a);
    for(i=0;i<7;i++)raw[2+i]=(uint8_t)((s[1+i*2]-'0')|((s[2+i*2]-'0')<<4));
}
static void print_hex(const uint8_t *v)
{
    static const char d[]="0123456789abcdef";char text[NV_LEN*2+1];unsigned i;
    for(i=0;i<NV_LEN;i++){text[2*i]=d[v[i]>>4];text[2*i+1]=d[v[i]&15];}text[NV_LEN*2]='\n';write_all(1,text,sizeof(text));
}
__attribute__((used)) static int main_c(int argc,char **argv)
{
    uint8_t before[NV_LEN],after[NV_LEN];long fd;int client,writing,result=1;
    if(argc==2&&equal(argv[1],"selftest")) {
        static const uint8_t expected[9]={0x08,0x4a,0x09,0x51,0x24,0x30,0x32,0x57,0x81};
        memset(before,0x5a,sizeof(before));encode("490154203237518",before);
        return valid_imei("490154203237518")&&!valid_imei("490154203237517")
          &&same(before,expected,sizeof(expected))&&before[9]==0x5a?0:1;
    }
    if(argc<2||(!equal(argv[1],"read")&&!equal(argv[1],"restore")))return fail("usage: hh71-imei read | restore 15_DIGIT_ORIGINAL_IMEI");
    writing=equal(argv[1],"restore");if((!writing&&argc!=2)||(writing&&(argc!=3||!valid_imei(argv[2]))))return fail("IMEI must be 15 digits with a valid Luhn check digit");
    fd=sc3(SYS_OPEN,(long)"/dev/diag",O_RDWR|O_NONBLOCK,0);if(fd<0)return fail("cannot open /dev/diag");
    client=register_dci((int)fd);if(client<=0||client>=DCI_OK){sc1(SYS_CLOSE,fd);return fail("cannot register DCI client");}
    if(nv_request((int)fd,client,0,0,before)<0)goto done;
    if(!writing){print_hex(before);result=0;goto done;}
    copy(after,before,NV_LEN);encode(argv[2],after);
    if(nv_request((int)fd,client,1,after,before)<0||!same(before,after,NV_LEN))goto done;
    if(nv_request((int)fd,client,0,0,before)<0||!same(before,after,NV_LEN))goto done;
    print_hex(before);result=0;
done:
    sc3(SYS_IOCTL,fd,DCI_DEINIT,(long)&client);sc1(SYS_CLOSE,fd);
    return result?fail("NV 550 request failed, timed out or did not verify"):0;
}
#ifdef HOST_TEST
int main(int argc,char **argv) { return main_c(argc,argv); }
#else
__attribute__((naked,noreturn)) void _start(void)
{__asm__ volatile("ldr r0,[sp]\nadd r1,sp,#4\nbl main_c\nmov r7,#1\nsvc 0\n":::"memory");__builtin_unreachable();}
#endif
