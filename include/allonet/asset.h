//
//  asset.h
//  allonet
//
//  Created by Patrik on 2020-06-12.
//

#ifndef asset_h
#define asset_h


typedef struct asset_job {
    //- asset meta
    char *asset_id;
    
    //- job state
    int offset;
    
    //- network settings
    int network_max_accept_bytes;
    
    //- buffers
    char *buffer;
    int buffer_size;
    
    //- callbacks
    int(*request_data)(char *asset_id, char *buffer, int offset, int buffer_size);
    
    void(*send_data)(char *buffer, int count);
} asset_job;

void asset_send_init(asset_job *job);

void asset_send_work(asset_job *job);

#endif /* asset_h */
