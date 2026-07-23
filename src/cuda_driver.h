#ifndef VEXIUM_CUDA_DRIVER_H
#define VEXIUM_CUDA_DRIVER_H

#include "common.h"

typedef struct {
    bool is_available;
    char device_name[256];
    size_t total_vram_mb;
    int compute_capability_major;
    int compute_capability_minor;
} CUDADriverInfo;

bool cuda_driver_init(CUDADriverInfo* info);
void cuda_driver_print_status(const CUDADriverInfo* info);

#endif
