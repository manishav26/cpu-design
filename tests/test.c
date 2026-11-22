#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "assembler.h"
#include "loader.h"
#include "trace.h"

static void equal(const CPU *a, const CPU *b)
{
    assert(!memcmp(a->regs,b->regs,sizeof a->regs));
    assert(!memcmp(a->ram,b->ram,sizeof a->ram));
    assert(a->pc==b->pc && a->ir==b->ir && a->mar==b->mar && a->mdr==b->mdr);
    assert(a->sp==b->sp && a->z==b->z && a->n==b->n);
    assert(a->halted==b->halted && a->faulted==b->faulted && a->steps==b->steps);
}
static void example(const char *path, unsigned input, unsigned expected)
{
    static Program p;
    CPU normal, traced, before;
    char err[256], line[1024];
    unsigned snapshots=0, initial=0, halt=0, fetches=0, decodes=0, executes=0;
    assert(assemble_file(path,&p,err,sizeof err)==0);
    int input_addr=-1, result_addr=-1;
    for(int i=0;i<p.count;++i) {
        if(!strcmp(p.listing[i].label,"input")) input_addr=p.listing[i].addr;
        if(!strcmp(p.listing[i].label,"result")) result_addr=p.listing[i].addr;
    }
    assert(input_addr>=0 && result_addr>=0);
    p.mem[input_addr]=(uint16_t)input;
    assert(cpu_load_program(&normal,&p)==0);
    traced=normal; before=normal;
    FILE *out=tmpfile(); assert(out);
    TraceRange range={0,(unsigned)result_addr};
    TraceRange segments[] = {{TEXT_START,TEXT_END}, {DATA_START,DATA_END},
                             {HEAP_START,HEAP_END}, {STACK_START,STACK_END}};
    int recursive = strstr(path, "recursive") != NULL;
    const TraceRange *ranges = recursive ? segments : &range;
    size_t count = recursive ? 4 : 1;
    assert(trace_snapshot(out,&traced,-1,ranges,count)==0);
    equal(&before,&traced); /* formatter is read-only */
    fclose(out); out=tmpfile(); assert(out);
    assert(cpu_run(&normal,1000)==0);
    assert(trace_run(&traced,1000,out,ranges,count)==0);
    equal(&normal,&traced);
    assert(normal.halted && !normal.faulted && normal.steps<1000);
    assert(normal.sp==SP_START);
    assert(normal.regs[0]==expected && normal.ram[result_addr]==expected);
    rewind(out);
    while(fgets(line,sizeof line,out)) {
        if(!strncmp(line,"SNAPSHOT",8)) ++snapshots;
        if(strstr(line,"INITIAL")) ++initial;
        if(strstr(line,"word=0x0000 HALT")) ++halt;
        if(strstr(line," FETCH ")) ++fetches;
        if(strstr(line," DECODE ")) ++decodes;
        if(strstr(line," EXECUTE ")) ++executes;
    }
    assert(snapshots==normal.steps+1 && initial==1 && halt==1);
    assert(normal.clock==3*normal.steps);       /* FETCH, DECODE, EXECUTE */
    assert(fetches==normal.steps && decodes==normal.steps && executes==normal.steps);
    fclose(out);
    printf("PASS %s input=%u result=%u steps=%lu snapshots=%u\n",path,input,expected,normal.steps,snapshots);
}
int main(void)
{
    example("programs/square.asm",8,64);
    example("programs/factorial.asm",0,1);
    example("programs/factorial.asm",1,1);
    example("programs/factorial.asm",5,120);
    example("programs/factorial.asm",8,40320);
    example("programs/factorial_recursive.asm",0,1);
    example("programs/factorial_recursive.asm",1,1);
    example("programs/factorial_recursive.asm",4,24);
    example("programs/factorial_recursive.asm",8,40320);
    CPU cpu; cpu_reset(&cpu); cpu.ram[0]=0xA000; /* JMP 0 */
    assert(trace_run(&cpu,3,NULL,NULL,0)==-1 && cpu.faulted && !cpu.halted && cpu.steps==3);
    cpu_reset(&cpu); cpu.pc=0x40;
    assert(trace_run(&cpu,3,NULL,NULL,0)==-1 && cpu.faulted && cpu.steps==0);
    puts("PASS instruction limit and CPU fault propagation");
    return 0;
}