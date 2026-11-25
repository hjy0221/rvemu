#include "rvemu.h"

#include <assert.h>
#include <stdio.h>

static void put(rvemu *c, const uint32_t *p, size_t n) { assert(rvemu_load_bytes(c,0,p,n*sizeof(*p))); }

int main(void) {
    rvemu c;
    assert(rvemu_init(&c, 256));
    {
        const uint32_t p[] = {0x00500093,0x00308113,0x402081b3,0x0020f233,0x0020e2b3,0x0020c333,0x00100073};
        put(&c,p,sizeof(p)/sizeof(*p));
        assert(rvemu_run(&c,20,NULL)==RVEMU_HALT);
        assert(c.x[1]==5 && c.x[2]==8 && c.x[3]==(uint32_t)-3);
        assert(c.x[4]==0 && c.x[5]==13 && c.x[6]==13 && c.x[0]==0);
    }
    rvemu_destroy(&c);
    assert(rvemu_init(&c,256));
    {
        /* Store/load word at 64, branch over a bad write, then JAL to ebreak. */
        const uint32_t p[] = {0x04000093,0x02a00113,0x0020a023,0x0000a183,
                              0x00218463,0x00100213,0x0080006f,0x00100073,0x00100073};
        put(&c,p,sizeof(p)/sizeof(*p));
        assert(rvemu_run(&c,20,NULL)==RVEMU_HALT);
        assert(c.x[3]==42 && c.x[4]==0 && c.x[0]==0);
    }
    rvemu_destroy(&c);
    assert(rvemu_init(&c,64));
    {
        const uint32_t p[] = {0x00100093,0x00000000}; put(&c,p,2);
        assert(rvemu_step(&c)==RVEMU_OK);
        assert(rvemu_step(&c)==RVEMU_ERR_INSTRUCTION);
        assert(c.x[0]==0 && c.x[1]==1);
    }
    rvemu_destroy(&c);
    puts("all rvemu tests passed");
    return 0;
}
