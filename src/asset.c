//
//  asset.c
//  allocubeappliance
//
//  Created by Patrik on 2020-06-12.
//

#include <stdio.h>
#include <allonet/asset.h>
#include <stdlib.h>

#define min(a, b) a < b ? a : b;


void asset_send_work(asset_job *job) {
    // Network can accept more data?
    int count_max = min(job->network_max_accept_bytes, job->buffer_size);
    
    // Request data from host
    int len = job->request_data(job->asset_id, job->buffer, job->offset, count_max);
    
    // Send the data to the network
    if (len > 0) {
        job->send_data(job->buffer, len);
    } else {
        printf("no data read");
    }
}

