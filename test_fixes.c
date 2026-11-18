
#include <stdio.h>
#include "src/cpu.h"

int main(void)
{
    CPU cpu;

    cpu_reset(&cpu);
    cpu.regs[0] = 65535;
    cpu.regs[1] = 65535;
    cpu.ram[0] = 0x7100;
    cpu.ram[1] = 0x0000;

    int mul_status = cpu_run(&cpu, 10);
    printf("MUL: %s\n",
           mul_status == 0 && cpu.regs[0] == 1
           ? "PASS" : "FAIL");

    cpu_reset(&cpu);
    cpu.regs[1] = 65535;
    cpu.ram[0] = 0x21C1;

    int load_status = cpu_run(&cpu, 10);
    printf("LOAD bounds: %s\n",
           load_status == -1 && cpu.faulted
           ? "PASS" : "FAIL");

    cpu_reset(&cpu);
    cpu.regs[1] = 65535;
    cpu.ram[0] = 0x31C1;

    int store_status = cpu_run(&cpu, 10);
    printf("STORE bounds: %s\n",
           store_status == -1 && cpu.faulted
           ? "PASS" : "FAIL");

    return 0;
}
