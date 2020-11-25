//
//  _asset.h
//  allonet
//
//  Created by Patrik on 2020-11-25.
//

#ifndef _asset_h
#define _asset_h

int asset_read_header(uint8_t const **data, size_t *data_length, uint16_t *out_mid, cJSON const **out_json);

#endif /* _asset_h */
