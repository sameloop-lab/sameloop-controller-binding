// SPDX-License-Identifier: MIT
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed: %s at line %d\n",#x,__LINE__); return EXIT_FAILURE; } } while (0)
int main(void) {

    CHECK(control_duty(true,true,32.4,25)==100);
    CHECK(fabs(control_duty(true,true,27,25)-40)<1e-8);
    CHECK(control_duty(true,true,20,25)==0);
    CHECK(control_duty(false,true,55,25)==0);
    CHECK(control_duty(true,false,55,25)==0);
    CHECK(sample_available(true,25,10,10.999));
    CHECK(!sample_available(true,25,10,11));
    CHECK(!sample_available(false,25,10,10.1));
    CHECK(!sample_available(true,NAN,10,10.1));
    CHECK(!sample_available(true,INFINITY,10,10.1));
    CHECK(!sample_available(true,151,10,10.1));
    CHECK(!sample_available(true,25,0,10));
    CHECK(!sample_available(true,25,11,10));

    puts("model tests passed"); return 0;
}
