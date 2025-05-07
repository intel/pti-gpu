//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <CL/sycl.hpp>
#include <iostream>
#ifdef __linux__
	#include <unistd.h>
#endif

using namespace sycl;
int main(){
	int GLOBAL_SIZE = 64 * 512;
        queue q{gpu_selector(), {property::queue::in_order()}};
        float *a = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
        float *b = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
        float *c = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
        float *d = (float *)malloc_host(sizeof(float)*GLOBAL_SIZE, q);
        float *e = (float *)malloc_host(sizeof(float)*GLOBAL_SIZE, q);
        for(int i = 0; i < GLOBAL_SIZE; i++) {
                a[i] = i;
                b[i] = i;
                c[i] = 0;
	}
        for (int m = 0; m < 5; m++) {
        	int WORKGROUP_SIZE = 64;
        	q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
                	int index = i.get_local_id();
                	c[0] = index;
        	}).wait();

        	q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
                	int index = i.get_group_linear_id();
                	c[0] = index;
        	}).wait();

        	
        	q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
                	int index = i.get_global_id();
			c[index] = a[index] + b[index];
        	}).wait();

        	//setenv("PTI_ENABLE_COLLECTION", "0", 1);

        	WORKGROUP_SIZE = WORKGROUP_SIZE * 2;
	        q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
        	        int index = i.get_global_id();
                	float f[128];
                
                	float x = a[index];
                	float y = b[index];

                	for (int k = 0; k < 20; k++) {
                  		for (int i = 0; i < 64; i++) {
                    			f[i] = x + y + i;
                  		}

		  		for (int i = 0; i < 8; i++) {
                    			for (int j = 0; j < 64; j++) {
                      				x  = x * f[j];
                      				y  = y * f[j];
                    			}
		  		}
		  		c[index] = x + y;
                	}
        	}).wait();


        }

        d[0] = -1;
        e[0] = 0;
        q.wait();
    
	q.submit([&](sycl::handler &cgh) {
          cgh.host_task([=] { std::memcpy(e, d, GLOBAL_SIZE); });
        });
        q.wait();
        q.memcpy(e, d, GLOBAL_SIZE);
        q.wait();

        q.wait();

        free((void *)a, q);
        free((void *)b, q);
        free((void *)c, q);

        return 0;
}
