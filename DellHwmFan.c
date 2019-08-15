#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>

// SMM Part
#define DISABLE_BIOS_METHOD1 0x30a3
#define ENABLE_BIOS_METHOD1  0x31a3
#define DISABLE_BIOS_METHOD2 0x34a3
#define ENABLE_BIOS_METHOD2  0x35a3
#define ENABLE_FN            0x32a3
#define FN_STATUS            0x0025

#define GET_FAN_LVL          0x00a3
#define SET_FAN_LVL          0x01a3
#define GET_FAN_RPM          0x02a3

#define GET_FAN_NOR          0x04a3
#define GET_FAN_TOL          0x05a3

#define GET_SENSOR           0x10a3


struct smm_regs {
    unsigned int eax;
    unsigned int ebx __attribute__ ((packed));
    unsigned int ecx __attribute__ ((packed));
    unsigned int edx __attribute__ ((packed));
    unsigned int esi __attribute__ ((packed));
    unsigned int edi __attribute__ ((packed));
};

static int i8k_smm(struct smm_regs *regs)
{
    int rc;
    int eax = regs->eax;

    asm volatile(
            "pushq %%rax\n\t"
            "movl 0(%%rax),%%edx\n\t"
            "pushq %%rdx\n\t"
            "movl 4(%%rax),%%ebx\n\t"
            "movl 8(%%rax),%%ecx\n\t"
            "movl 12(%%rax),%%edx\n\t"
            "movl 16(%%rax),%%esi\n\t"
            "movl 20(%%rax),%%edi\n\t"
            "popq %%rax\n\t"
            "out %%al,$0xb2\n\t"
            "out %%al,$0x84\n\t"
            "xchgq %%rax,(%%rsp)\n\t"
            "movl %%ebx,4(%%rax)\n\t"
            "movl %%ecx,8(%%rax)\n\t"
            "movl %%edx,12(%%rax)\n\t"
            "movl %%esi,16(%%rax)\n\t"
            "movl %%edi,20(%%rax)\n\t"
            "popq %%rdx\n\t"
            "movl %%edx,0(%%rax)\n\t"
            "pushfq\n\t"
            "popq %%rax\n\t"
            "andl $1,%%eax\n"
            :"=a"(rc)
            :    "a"(regs)
            :    "%ebx", "%ecx", "%edx", "%esi", "%edi");

    if (rc != 0 || (regs->eax & 0xffff) == 0xffff || regs->eax == eax)
            return -1;

    return 0;
}


int send(unsigned int cmd, unsigned int arg) {

    struct smm_regs regs = { .eax = cmd, };

    regs.ebx = arg;

    i8k_smm(&regs);
    return regs.eax ;

}

/* sets the speed and returns the speed of the fan after that */
int set_speed(int speed) {

    if ( speed == 0 ) {
        send(SET_FAN_LVL, 0x0000);
    } else if ( speed == 1 ) {
        send(SET_FAN_LVL, 0x0100);
    } else if ( speed == 2 ) {
        send(SET_FAN_LVL, 0x0200);
    } else if ( speed == 3 ) {
		send(SET_FAN_LVL, 0x0300);
	} else {
        printf("Ignoring unknown speed: %d\n",speed);
    }
    // return the speed the fan has now
    return send(GET_FAN_LVL, 0);
}


int probecodes (void) {

    printf("Please tune the startcode by editing the source code\n");
    /* Remove the following exit call to enable this routine.
     *
     * *WARNING:* Proving for random codes in the SMBIOS management can
     * cause unexpected behaviour (even crashes or data loss) on your machine.
     *
     * USE AT YOUR OWN RISK!!
     */
    exit(EXIT_FAILURE);

    /* If you want to test this fast, use startcode=0x30a0 (for example)
     * and you should see that the code 0x30a3 is detected.
     * If you want to test all the codes then use startcode=0x0000 but
     * this will take a while.
     */
    int startcode = 0x0001;
    int fanstatus, trycode;

    // Set the speed to 2, sleep 3 seconds and get the speed of the fan
    set_speed(2);
    sleep(3);
    fanstatus = send(GET_FAN_LVL, 0);
    if (fanstatus == 2 ) {
        printf ("Your fan status is already set to full speed.\n"
                "In order for this to work, please set it to auto (enable BIOS control)\n"
                "And stop any process that is consuming CPU resources\n");
        exit(EXIT_FAILURE);
    }

    for (trycode=startcode; trycode <= 0xFFFF; trycode++) {
        printf ("Probing code: %#06x\n",trycode);
        // Send the code
        send(trycode,0);
        // Set the speed to 2, sleep 3 seconds and get the speed of the fan
        set_speed(2);
        sleep(3);
        fanstatus = send(GET_FAN_LVL, 0);
        // If the fan is still 2 this is because the previous code disabled
        // the BIOS control of the fan.
        // Please ensure that your system is idle when running this (otherwise
        // the BIOS could set the fan to max speed to cool down your system
        // because of the load)
        if (fanstatus == 2 ) {
            printf ("The code %#06x disabled the FAN control!!!\n",trycode);
            printf ("Enabling BIOS control back...\n");
            send(ENABLE_BIOS_METHOD1,0);
            sleep(3);
            fanstatus = send(GET_FAN_LVL, 0);
            if (fanstatus == 2 ) {
                printf ("ERROR: Unable to bring BIOS control back.\n");
                exit (EXIT_FAILURE);
            }
        }
    }
}

// HWM Part
unsigned short HwmIoAddr = 0x0000;
unsigned short UptIoAddr = 0x0000;

unsigned char GetHwmChar(unsigned short addr)
{
    unsigned char offset10 = inb(HwmIoAddr + 10);
    //printf("HwO10: %02X \n", offset10);

    outb(0x0000, HwmIoAddr + 10);
    outw(0x8001, HwmIoAddr + 2);
    outw(0x0102, HwmIoAddr + 4);
    outw(0x8006, HwmIoAddr + 2);
    outl(addr << 16,   HwmIoAddr + 4);

    outb(0x0001, HwmIoAddr);
    unsigned char subret = 0;
    unsigned char offset8 = 0;
    unsigned char subret1 = 0;
    do{
        subret = inb(HwmIoAddr + 8);
        offset8 = subret & 0x71;
    }
    while(!offset8);
    outb(offset8, HwmIoAddr + 8);
    //printf("HwO8: %02X \n", offset8);
    if(offset8 & 1){
        subret1 = inb(HwmIoAddr + 1);
        //printf("HwO1: %02X \n", subret1);
    }

    unsigned char ret = 0;
    if(subret1 == 1){
        outw(0x8004, HwmIoAddr + 2);
        ret = inb(HwmIoAddr + 4);
        //printf("Read HwIo 0x%04X : 0x%02X\n", addr, ret);
    }

    outb(offset10, HwmIoAddr + 10);
    return ret;
}

unsigned short GetHwmShort(unsigned short addr) {
    return GetHwmChar(addr) | ((unsigned short)GetHwmChar(addr + 1) << 8);
}

int SetHwmChar(unsigned short addr, unsigned char val)
{
    unsigned char offset10 = inb(HwmIoAddr + 10);
    //printf("HwO10: %02X \n", offset10);

    outb(0x0000, HwmIoAddr + 10);
    outw(0x8001, HwmIoAddr + 2);
    outw(0x0103, HwmIoAddr + 4);
    outw(0x8006, HwmIoAddr + 2);
    outl((addr << 16) | val, HwmIoAddr + 4);

    outb(0x0001, HwmIoAddr);
    unsigned char subret = 0;
    unsigned char offset8 = 0;
    unsigned char subret1 = 0;
    do{
        subret = inb(HwmIoAddr + 8);
        offset8 = subret & 0x71;
    }
    while(!offset8);
    outb(offset8, HwmIoAddr + 8);
    //printf("HwO8: %02X \n", offset8);
    if(offset8 & 1){
        subret1 = inb(HwmIoAddr + 1);
        //printf("HwO1: %02X \n", subret1);
    }

    unsigned char ret = 0;
    if(subret1 == 1){
        //outw(0x8004, HwmIoAddr + 2);
        //ret = inb(HwmIoAddr + 4);
        //printf("Set HwIo 0x%04X : 0x%02X\n", addr, val);
    }

    outb(offset10, HwmIoAddr + 10);
    return ret;
}

unsigned short GetPortAddr(unsigned char raddr)
{
    outb(0x55, 0x2E);
    outb(0x07, 0x2E);
    outb(0x0C, 0x2F);
    outb(raddr + 1, 0x2E);

    unsigned char AddrH8 = inb(0x2F);

    outb(raddr, 0x2E);
    unsigned char AddrL8 = inb(0x2F);

    outb(0xAA, 0x2E);

    return (unsigned short)(AddrH8 << 8) | AddrL8;
}

int init_ioperm(void)
{
    if (ioperm(0xb2, 4, 1))
        perror("ioperm:");
    if (ioperm(0x84, 4, 1))
        perror("ioperm:");

    if (ioperm(0x2E, 4, 1))
        perror("ioperm 2E:");
    if (ioperm(0x2F, 4, 1))
        perror("ioperm 2F:");
    HwmIoAddr = GetPortAddr(0x66);

    if(HwmIoAddr != 0x0000){
        //printf("HwmIoAddr : %04X\n", HwmIoAddr);
        if (ioperm(HwmIoAddr, 16, 1))
            perror("ioperm HwIoAddr:");
    }

    UptIoAddr = GetPortAddr(0x72);
    if(UptIoAddr != 0x0000){
        //printf("UptIoAddr : %04X\n", UptIoAddr);
        if (ioperm(UptIoAddr, 32, 1))
            perror("ioperm UptIoAddr:");
    }

    return 0;
}



int main(int argc, char **argv) {
    int speed, disable;
    unsigned int tries;
    unsigned int xarg;

    if (geteuid() != 0) {
        printf("need root privileges\n");
        exit(EXIT_FAILURE);
    }

    init_ioperm();

    if (argc == 1) {
        printf ("FanStep : %d\n", (short)send(GET_FAN_LVL, 0));
        printf ("FanRPM  : %d %d\n", (short)send(GET_FAN_RPM, 0), (unsigned char)GetHwmChar(0x0036));
        printf ("FanTol  : %d\n", (short)send(GET_FAN_TOL, 0));
        printf ("CPU Temp: %d, HDD Temp: %d, UTemp: %d, \n",
            send(GET_SENSOR, 0),
            send(GET_SENSOR, 1),
            send(GET_SENSOR, 2)
        );
        system("cat /proc/cpuinfo|grep MHz");
        return 0;
    }

    speed = strtol(argv[1], 0, 16);

    if(speed < 4) {
        if (speed == 0){
            unsigned char DumpHwm[0x300];
            for (int i = 0; i < 0x300; i++) {
                DumpHwm[i] = GetHwmChar(i);
            }
            for (int i = 0; i < 0x300; i++) {
                switch (i % 0x10) {
                    case 0x00:
                        printf("0x%04X | %02X ", i, DumpHwm[i]);
                        break;
                    case 0x08:
                        printf("| %02X ", DumpHwm[i]);
                        break;
                    case 0x0F:
                        printf("%02X\n", DumpHwm[i]);
                        break;
                    default:
                        printf("%02X ", DumpHwm[i]);
                        break;
                }
            }
            return 0;
        }
        printf ("Setting speed to: %d\n", speed );
        printf ("Speed is now at: %d\n", set_speed(speed));
    }
    else if (argc == 2) {
        printf("Read HwIo 0x%04X : 0x%02X\n", speed, GetHwmChar(speed));
    }
    else if (argc == 3) {
        unsigned char val = strtol(argv[2], 0, 16);
        SetHwmChar(speed, val);
        printf("Set HwIo 0x%04X : 0x%02X\n", speed, val);
    }
    else {
        printf ("Args length error!\n");
    }
}
