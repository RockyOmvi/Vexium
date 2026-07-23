#ifndef VEXIUM_GPU_DRIVER_H
#define VEXIUM_GPU_DRIVER_H

#include "common.h"

typedef enum {
    GPU_BACKEND_NONE,
    GPU_BACKEND_CUDA,
    GPU_BACKEND_ROCM,
    GPU_BACKEND_METAL,
    GPU_BACKEND_VULKAN,
    GPU_BACKEND_OPENCL
} GPUBackend;

typedef struct {
    bool is_available;
    GPUBackend backend;
    char device_name[256];
    size_t total_vram_mb;
    int compute_units;
    void* native_context;
    void* native_command_queue;
} GPUDriverInfo;

bool gpu_driver_init(GPUDriverInfo* info);
void* gpu_alloc_device_memory(GPUDriverInfo* info, size_t size);
void gpu_free_device_memory(GPUDriverInfo* info, void* ptr);
bool gpu_memcpy_to_device(GPUDriverInfo* info, void* dst_dev, const void* src_host, size_t size);
bool gpu_memcpy_to_host(GPUDriverInfo* info, void* dst_host, const void* src_dev, size_t size);
void gpu_driver_print_status(const GPUDriverInfo* info);

#endif
