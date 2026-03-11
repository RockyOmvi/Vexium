/**
 * Vexium Tensor and Neural Network Implementation
 * =============================================
 * 
 * A comprehensive tensor library with neural network support for Vexium V2.
 * This implementation provides:
 * - Dense tensor operations (creation, reshaping, broadcasting)
 * - Element-wise operations (add, sub, mul, div, matmul)
 * - Activation functions (ReLU, Sigmoid, Tanh, Softmax)
 * - Neural network layers (Dense, Conv2D, Dropout, MaxPool)
 * - Optimizers (SGD, Adam, RMSprop)
 * - Model training and inference
 * 
 * Author: Vexium Development Team
 * Version: 1.0.0
 */

#include "tensor.h"
#include "gc.h"
#include "opcodes.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>

/* Define RAND_MAX if not available */
#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif

/* Define M_PI if not available */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════
 * PRIVATE HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Calculate total number of elements from shape array
 */
static size_t calculate_size(int ndim, const int* shape) {
    size_t size = 1;
    for (int i = 0; i < ndim; i++) {
        size *= (size_t)shape[i];
    }
    return size;
}

/**
 * Calculate strides from shape (row-major order)
 */
static void calculate_strides(int ndim, const int* shape, int* strides) {
    strides[ndim - 1] = 1;
    for (int i = ndim - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * shape[i + 1];
    }
}

/**
 * Convert linear index to multi-dimensional indices
 */
static void index_to_indices(size_t linear_idx, int ndim, 
                            const int* strides, int* indices) {
    int remaining = (int)linear_idx;
    for (int i = 0; i < ndim; i++) {
        indices[i] = remaining / strides[i];
        remaining = remaining % strides[i];
    }
}

/**
 * Convert multi-dimensional indices to linear index
 */
static size_t indices_to_index(const int* indices, int ndim, 
                               const int* strides) {
    size_t idx = 0;
    for (int i = 0; i < ndim; i++) {
        idx += (size_t)indices[i] * (size_t)strides[i];
    }
    return idx;
}

/**
 * Check if two tensors have compatible shapes for broadcasting
 */
static bool check_broadcast(const int* shape1, int ndim1, 
                           const int* shape2, int ndim2,
                           int* out_shape, int* out_ndim) {
    /* Start from the right (last dimension) */
    int i1 = ndim1 - 1;
    int i2 = ndim2 - 1;
    int o = 0;
    
    while (i1 >= 0 || i2 >= 0) {
        int dim1 = (i1 >= 0) ? shape1[i1] : 1;
        int dim2 = (i2 >= 0) ? shape2[i2] : 1;
        
        if (dim1 == dim2) {
            out_shape[o] = dim1;
        } else if (dim1 == 1) {
            out_shape[o] = dim2;
        } else if (dim2 == 1) {
            out_shape[o] = dim1;
        } else {
            return false; /* Cannot broadcast */
        }
        
        i1--;
        i2--;
        o++;
    }
    
    *out_ndim = o;
    
    /* Reverse the output shape */
    for (int i = 0; i < o / 2; i++) {
        int tmp = out_shape[i];
        out_shape[i] = out_shape[o - 1 - i];
        out_shape[o - 1 - i] = tmp;
    }
    
    return true;
}

/**
 * Free data pointer based on dtype
 */
static void free_tensor_data(Tensor* t) {
    if (!t->owns_data || t->data.f32_data == NULL) {
        return;
    }
    
    switch (t->dtype) {
        case TENSOR_FLOAT32:
            free(t->data.f32_data);
            break;
        case TENSOR_FLOAT64:
            free(t->data.f64_data);
            break;
        case TENSOR_INT32:
            free(t->data.i32_data);
            break;
        case TENSOR_INT64:
            free(t->data.i64_data);
            break;
        case TENSOR_UINT8:
            free(t->data.u8_data);
            break;
        case TENSOR_BOOL:
            free(t->data.b_data);
            break;
    }
    
    t->data.f32_data = NULL;
}

/**
 * Allocate data pointer based on dtype
 */
static void* allocate_tensor_data(TensorDType dtype, size_t size) {
    switch (dtype) {
        case TENSOR_FLOAT32:
            return calloc(size, sizeof(float));
        case TENSOR_FLOAT64:
            return calloc(size, sizeof(double));
        case TENSOR_INT32:
            return calloc(size, sizeof(int32_t));
        case TENSOR_INT64:
            return calloc(size, sizeof(int64_t));
        case TENSOR_UINT8:
            return calloc(size, sizeof(uint8_t));
        case TENSOR_BOOL:
            return calloc(size, sizeof(bool));
        default:
            return NULL;
    }
}

static bool tensor_same_shape(Tensor* a, Tensor* b) {
    if (a == NULL || b == NULL) return false;
    if (a->dtype != b->dtype || a->ndim != b->ndim || a->size != b->size) return false;
    for (int i = 0; i < a->ndim; i++) {
        if (a->shape[i] != b->shape[i]) return false;
    }
    return true;
}

static Tensor* tensor_zeros_like(Tensor* src) {
    if (src == NULL) return NULL;
    return tensor_zeros(src->dtype, src->ndim, src->shape);
}

static bool write_exact(const void* ptr, size_t size, size_t count, FILE* f) {
    return ptr != NULL && f != NULL && fwrite(ptr, size, count, f) == count;
}

static bool read_exact(void* ptr, size_t size, size_t count, FILE* f) {
    return ptr != NULL && f != NULL && fread(ptr, size, count, f) == count;
}

/**
 * Get element size for a given dtype
 */
static size_t get_dtype_size(TensorDType dtype) {
    switch (dtype) {
        case TENSOR_FLOAT32:
            return sizeof(float);
        case TENSOR_FLOAT64:
            return sizeof(double);
        case TENSOR_INT32:
            return sizeof(int32_t);
        case TENSOR_INT64:
            return sizeof(int64_t);
        case TENSOR_UINT8:
            return sizeof(uint8_t);
        case TENSOR_BOOL:
            return sizeof(bool);
        default:
            return 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * TENSOR CREATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Create a new tensor with the given shape
 * 
 * This function allocates memory for a new tensor and its data.
 * The tensor owns its data and will free it when destroyed.
 * 
 * @param dtype    The data type for the tensor
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @return         Newly allocated tensor
 */
Tensor* tensor_new(TensorDType dtype, int ndim, const int* shape) {
    Tensor* t = (Tensor*)malloc(sizeof(Tensor));
    if (t == NULL) {
        fprintf(stderr, "[TENSOR] Failed to allocate tensor\n");
        return NULL;
    }
    
    /* Initialize object header */
    t->obj.type = OBJ_TENSOR;
    t->obj.next = NULL;
    t->obj.is_marked = false;
    
    /* Set properties */
    t->dtype = dtype;
    t->ndim = ndim;
    t->size = calculate_size(ndim, shape);
    t->owns_data = true;
    
    /* Allocate shape and strides arrays */
    t->shape = (int*)malloc(ndim * sizeof(int));
    t->strides = (int*)malloc(ndim * sizeof(int));
    
    if (t->shape == NULL || t->strides == NULL) {
        fprintf(stderr, "[TENSOR] Failed to allocate shape/strides\n");
        free(t->shape);
        free(t->strides);
        free(t);
        return NULL;
    }
    
    /* Copy shape and calculate strides */
    memcpy(t->shape, shape, ndim * sizeof(int));
    calculate_strides(ndim, shape, t->strides);
    
    /* Allocate data */
    t->data.f32_data = (float*)allocate_tensor_data(dtype, t->size);
    if (t->data.f32_data == NULL) {
        fprintf(stderr, "[TENSOR] Failed to allocate tensor data\n");
        free(t->shape);
        free(t->strides);
        free(t);
        return NULL;
    }
    
    return t;
}

/**
 * Create a tensor from existing data (borrows the data)
 * 
 * The tensor does not own the data - it will not free it when destroyed.
 * Use this for wrapping external data.
 * 
 * @param dtype    The data type
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @param data     Pointer to existing data
 * @return         Newly created tensor borrowing the data
 */
Tensor* tensor_from_data(TensorDType dtype, int ndim, const int* shape, void* data) {
    Tensor* t = tensor_new(dtype, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Free the allocated data and use the provided data */
    free_tensor_data(t);
    
    t->data.f32_data = (float*)data;
    t->owns_data = false;
    
    return t;
}

/**
 * Create a zero-initialized tensor
 * 
 * @param dtype    The data type
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @return         Zero-filled tensor
 */
Tensor* tensor_zeros(TensorDType dtype, int ndim, const int* shape) {
    Tensor* t = tensor_new(dtype, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Data is already zeroed by calloc in allocate_tensor_data */
    return t;
}

/**
 * Create a tensor filled with ones
 * 
 * @param dtype    The data type
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @return         Ones-filled tensor
 */
Tensor* tensor_ones(TensorDType dtype, int ndim, const int* shape) {
    Tensor* t = tensor_new(dtype, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Fill with ones */
    switch (dtype) {
        case TENSOR_FLOAT32:
            for (size_t i = 0; i < t->size; i++) {
                t->data.f32_data[i] = 1.0f;
            }
            break;
        case TENSOR_FLOAT64:
            for (size_t i = 0; i < t->size; i++) {
                t->data.f64_data[i] = 1.0;
            }
            break;
        case TENSOR_INT32:
            for (size_t i = 0; i < t->size; i++) {
                t->data.i32_data[i] = 1;
            }
            break;
        case TENSOR_INT64:
            for (size_t i = 0; i < t->size; i++) {
                t->data.i64_data[i] = 1;
            }
            break;
        case TENSOR_UINT8:
            for (size_t i = 0; i < t->size; i++) {
                t->data.u8_data[i] = 1;
            }
            break;
        case TENSOR_BOOL:
            for (size_t i = 0; i < t->size; i++) {
                t->data.b_data[i] = true;
            }
            break;
    }
    
    return t;
}

/**
 * Create a tensor filled with a constant value
 * 
 * @param dtype    The data type
 * @param value    The fill value
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @return         Constant-filled tensor
 */
Tensor* tensor_full(TensorDType dtype, double value, int ndim, const int* shape) {
    Tensor* t = tensor_new(dtype, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Fill with value */
    switch (dtype) {
        case TENSOR_FLOAT32:
            for (size_t i = 0; i < t->size; i++) {
                t->data.f32_data[i] = (float)value;
            }
            break;
        case TENSOR_FLOAT64:
            for (size_t i = 0; i < t->size; i++) {
                t->data.f64_data[i] = value;
            }
            break;
        case TENSOR_INT32:
            for (size_t i = 0; i < t->size; i++) {
                t->data.i32_data[i] = (int32_t)value;
            }
            break;
        case TENSOR_INT64:
            for (size_t i = 0; i < t->size; i++) {
                t->data.i64_data[i] = (int64_t)value;
            }
            break;
        case TENSOR_UINT8:
            for (size_t i = 0; i < t->size; i++) {
                t->data.u8_data[i] = (uint8_t)value;
            }
            break;
        case TENSOR_BOOL:
            for (size_t i = 0; i < t->size; i++) {
                t->data.b_data[i] = (value != 0.0);
            }
            break;
    }
    
    return t;
}

/**
 * Create an identity matrix (2D tensor)
 * 
 * @param size     Size of the square matrix
 * @param dtype    The data type
 * @return         Identity matrix tensor
 */
Tensor* tensor_identity(int size, TensorDType dtype) {
    int shape[2] = {size, size};
    Tensor* t = tensor_zeros(dtype, 2, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Fill diagonal with 1s */
    for (int i = 0; i < size; i++) {
        int indices[2] = {i, i};
        tensor_set_scalar(t, indices, 1.0);
    }
    
    return t;
}

/**
 * Create a tensor with random values (uniform distribution [0, 1])
 * 
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @return         Tensor with random float32 values
 */
Tensor* tensor_rand(int ndim, const int* shape) {
    Tensor* t = tensor_new(TENSOR_FLOAT32, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Initialize random number generator */
    static bool rng_initialized = false;
    if (!rng_initialized) {
        srand((unsigned int)time(NULL));
        rng_initialized = true;
    }
    
    /* Fill with random values */
    for (size_t i = 0; i < t->size; i++) {
        t->data.f32_data[i] = (float)rand() / (float)RAND_MAX;
    }
    
    return t;
}

/**
 * Create a tensor with normally distributed random values
 * 
 * Uses Box-Muller transform for normal distribution.
 * 
 * @param ndim     Number of dimensions
 * @param shape    Array of dimension sizes
 * @param mean     Mean of the distribution
 * @param std      Standard deviation
 * @return         Tensor with normal random values
 */
Tensor* tensor_randn(int ndim, const int* shape, double mean, double std) {
    Tensor* t = tensor_new(TENSOR_FLOAT32, ndim, shape);
    if (t == NULL) {
        return NULL;
    }
    
    /* Initialize random number generator */
    static bool rng_initialized = false;
    if (!rng_initialized) {
        srand((unsigned int)time(NULL));
        rng_initialized = true;
    }
    
    /* Fill with normally distributed values using Box-Muller */
    for (size_t i = 0; i < t->size; i += 2) {
        /* Generate two independent normal random numbers */
        double u1 = (double)rand() / (double)RAND_MAX;
        double u2 = (double)rand() / (double)RAND_MAX;
        if (u1 < 1e-12) u1 = 1e-12;
        
        /* Box-Muller transform */
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
        
        t->data.f32_data[i] = (float)(mean + std * z0);
        
        if (i + 1 < t->size) {
            t->data.f32_data[i + 1] = (float)(mean + std * z1);
        }
    }
    
    return t;
}

/**
 * Clone a tensor (deep copy)
 * 
 * @param t        Source tensor to clone
 * @return         New tensor with copied data
 */
Tensor* tensor_clone(Tensor* t) {
    if (t == NULL) {
        return NULL;
    }
    
    Tensor* clone = tensor_new(t->dtype, t->ndim, t->shape);
    if (clone == NULL) {
        return NULL;
    }
    
    /* Copy data */
    size_t data_size = t->size * get_dtype_size(t->dtype);
    memcpy(clone->data.f32_data, t->data.f32_data, data_size);
    
    return clone;
}

/* ═══════════════════════════════════════════════════════════════════
 * TENSOR SHAPE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Reshape a tensor to new dimensions
 * 
 * Returns a new tensor with the same data but different shape.
 * The original tensor is unchanged.
 * 
 * @param t            Source tensor
 * @param new_ndim     New number of dimensions
 * @param new_shape    New shape array
 * @return            Reshaped tensor (new allocation)
 */
Tensor* tensor_reshape(Tensor* t, int new_ndim, const int* new_shape) {
    if (t == NULL) {
        return NULL;
    }
    
    /* Calculate new size */
    size_t new_size = calculate_size(new_ndim, new_shape);
    
    /* Check size match */
    if (new_size != t->size) {
        fprintf(stderr, "[TENSOR] Reshape size mismatch: %zu -> %zu\n", 
                t->size, new_size);
        return NULL;
    }
    
    /* Create new tensor with new shape but borrowed data */
    Tensor* reshaped = tensor_from_data(t->dtype, new_ndim, new_shape, t->data.f32_data);
    if (reshaped == NULL) {
        return NULL;
    }
    
    /* Actually we need to copy the data since we're borrowing */
    reshaped->owns_data = true;
    reshaped->data.f32_data = (float*)allocate_tensor_data(t->dtype, t->size);
    if (reshaped->data.f32_data == NULL) {
        free(reshaped);
        return NULL;
    }
    
    size_t data_size = t->size * get_dtype_size(t->dtype);
    memcpy(reshaped->data.f32_data, t->data.f32_data, data_size);
    
    return reshaped;
}

/**
 * Transpose a tensor (swap dimensions)
 * 
 * For 2D tensors, this is a standard matrix transpose.
 * For higher dimensions, reverses all axes.
 * 
 * @param t        Source tensor
 * @return         Transposed tensor
 */
Tensor* tensor_transpose(Tensor* t) {
    if (t == NULL) {
        return NULL;
    }
    
    /* Create new shape (reversed) */
    int* new_shape = (int*)malloc(t->ndim * sizeof(int));
    if (new_shape == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < t->ndim; i++) {
        new_shape[i] = t->shape[t->ndim - 1 - i];
    }
    
    /* Create new tensor */
    Tensor* result = tensor_new(t->dtype, t->ndim, new_shape);
    free(new_shape);
    
    if (result == NULL) {
        return NULL;
    }
    
    /* Transpose the data */
    int* indices = (int*)malloc(t->ndim * sizeof(int));
    int* result_indices = (int*)malloc(t->ndim * sizeof(int));
    
    if (indices == NULL || result_indices == NULL) {
        free(indices);
        free(result_indices);
        tensor_free(result);
        return NULL;
    }
    
    for (size_t i = 0; i < t->size; i++) {
        index_to_indices(i, t->ndim, t->strides, indices);
        
        /* Reverse indices for transpose */
        for (int j = 0; j < t->ndim; j++) {
            result_indices[j] = indices[t->ndim - 1 - j];
        }
        
        size_t result_idx = indices_to_index(result_indices, t->ndim, result->strides);
        
        /* Copy element */
        double value = tensor_get_scalar(t, indices);
        tensor_set_scalar(result, result_indices, value);
    }
    
    free(indices);
    free(result_indices);
    
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 * TENSOR ELEMENT ACCESS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Get scalar value at indices (as double)
 * 
 * @param t        Tensor
 * @param indices  Array of indices for each dimension
 * @return         Value at the specified indices
 */
double tensor_get_scalar(Tensor* t, const int* indices) {
    if (t == NULL || indices == NULL) {
        return 0.0;
    }
    
    size_t idx = indices_to_index(indices, t->ndim, t->strides);
    
    switch (t->dtype) {
        case TENSOR_FLOAT32:
            return (double)t->data.f32_data[idx];
        case TENSOR_FLOAT64:
            return t->data.f64_data[idx];
        case TENSOR_INT32:
            return (double)t->data.i32_data[idx];
        case TENSOR_INT64:
            return (double)t->data.i64_data[idx];
        case TENSOR_UINT8:
            return (double)t->data.u8_data[idx];
        case TENSOR_BOOL:
            return t->data.b_data[idx] ? 1.0 : 0.0;
        default:
            return 0.0;
    }
}

/**
 * Set scalar value at indices (from double)
 * 
 * @param t        Tensor
 * @param indices  Array of indices for each dimension
 * @param value    Value to set
 */
void tensor_set_scalar(Tensor* t, const int* indices, double value) {
    if (t == NULL || indices == NULL) {
        return;
    }
    
    size_t idx = indices_to_index(indices, t->ndim, t->strides);
    
    switch (t->dtype) {
        case TENSOR_FLOAT32:
            t->data.f32_data[idx] = (float)value;
            break;
        case TENSOR_FLOAT64:
            t->data.f64_data[idx] = value;
            break;
        case TENSOR_INT32:
            t->data.i32_data[idx] = (int32_t)value;
            break;
        case TENSOR_INT64:
            t->data.i64_data[idx] = (int64_t)value;
            break;
        case TENSOR_UINT8:
            t->data.u8_data[idx] = (uint8_t)value;
            break;
        case TENSOR_BOOL:
            t->data.b_data[idx] = (value != 0.0);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * ELEMENT-WISE OPERATIONS (In-place)
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * In-place element-wise addition: t1 += t2
 * 
 * Supports broadcasting if shapes are compatible.
 * 
 * @param t1       First tensor (modified in place)
 * @param t2       Second tensor to add
 */
void tensor_iadd(Tensor* t1, Tensor* t2) {
    if (t1 == NULL || t2 == NULL) return;
    if (t1->dtype != t2->dtype) return;
    
    /* Check for broadcasting */
    if (t1->ndim == t2->ndim) {
        /* Same shape - direct element-wise */
        for (size_t i = 0; i < t1->size; i++) {
            t1->data.f32_data[i] += t2->data.f32_data[i];
        }
    } else {
        /* Try broadcasting */
        int broadcast_shape[16]; /* Max 16 dimensions */
        int broadcast_ndim;
        
        if (!check_broadcast(t1->shape, t1->ndim, t2->shape, t2->ndim,
                             broadcast_shape, &broadcast_ndim)) {
            fprintf(stderr, "[TENSOR] Cannot broadcast shapes for addition\n");
            return;
        }
        
        /* Perform broadcast addition */
        int* t1_indices = (int*)malloc(t1->ndim * sizeof(int));
        int* t2_indices = (int*)malloc(t2->ndim * sizeof(int));
        int* broadcast_indices = (int*)malloc(broadcast_ndim * sizeof(int));
        
        if (t1_indices == NULL || t2_indices == NULL || broadcast_indices == NULL) {
            free(t1_indices);
            free(t2_indices);
            free(broadcast_indices);
            return;
        }
        
        for (size_t i = 0; i < t1->size; i++) {
            index_to_indices(i, t1->ndim, t1->strides, t1_indices);
            
            /* Get corresponding t2 index using broadcast */
            bool valid = true;
            for (int j = 0; j < broadcast_ndim; j++) {
                int dim = broadcast_ndim - 1 - j;
                int t1_dim = t1->ndim - 1 - j;
                int t2_dim = t2->ndim - 1 - j;
                
                if (t1_dim >= 0 && t1->shape[t1_dim] == broadcast_shape[dim]) {
                    broadcast_indices[j] = t1_indices[t1_dim];
                } else if (t2_dim >= 0 && t2->shape[t2_dim] == broadcast_shape[dim]) {
                    broadcast_indices[j] = 0; /* Will broadcast */
                }
            }
            
            /* Calculate t2 index */
            if (t2->ndim > 0) {
                size_t t2_idx = 0;
                for (int j = 0; j < t2->ndim; j++) {
                    int t2_shape_idx = t2->ndim - 1 - j;
                    int broadcast_idx = broadcast_ndim - 1 - j;
                    if (broadcast_idx >= 0 && t2->shape[t2_shape_idx] == broadcast_shape[broadcast_idx]) {
                        t2_indices[t2_shape_idx] = broadcast_indices[broadcast_idx];
                    } else {
                        t2_indices[t2_shape_idx] = 0;
                    }
                }
                t2_idx = indices_to_index(t2_indices, t2->ndim, t2->strides);
                
                t1->data.f32_data[i] += t2->data.f32_data[t2_idx];
            }
        }
        
        free(t1_indices);
        free(t2_indices);
        free(broadcast_indices);
    }
}

/**
 * In-place element-wise subtraction: t1 -= t2
 * 
 * @param t1       First tensor (modified in place)
 * @param t2       Second tensor to subtract
 */
void tensor_isub(Tensor* t1, Tensor* t2) {
    if (t1 == NULL || t2 == NULL) return;
    if (t1->dtype != t2->dtype || t1->size != t2->size) return;
    
    for (size_t i = 0; i < t1->size; i++) {
        t1->data.f32_data[i] -= t2->data.f32_data[i];
    }
}

/**
 * In-place element-wise multiplication: t1 *= t2
 * 
 * @param t1       First tensor (modified in place)
 * @param t2       Second tensor to multiply
 */
void tensor_imul(Tensor* t1, Tensor* t2) {
    if (t1 == NULL || t2 == NULL) return;
    if (t1->dtype != t2->dtype || t1->size != t2->size) return;
    
    for (size_t i = 0; i < t1->size; i++) {
        t1->data.f32_data[i] *= t2->data.f32_data[i];
    }
}

/**
 * In-place element-wise division: t1 /= t2
 * 
 * @param t1       First tensor (modified in place)
 * @param t2       Second tensor (divisor)
 */
void tensor_idiv(Tensor* t1, Tensor* t2) {
    if (t1 == NULL || t2 == NULL) return;
    if (t1->dtype != t2->dtype || t1->size != t2->size) return;
    
    for (size_t i = 0; i < t1->size; i++) {
        if (t2->data.f32_data[i] != 0.0f) {
            t1->data.f32_data[i] /= t2->data.f32_data[i];
        }
    }
}

/**
 * In-place scalar addition: t += scalar
 * 
 * @param t        Tensor (modified in place)
 * @param scalar   Scalar value to add
 */
void tensor_iadd_scalar(Tensor* t, double scalar) {
    if (t == NULL) return;
    
    for (size_t i = 0; i < t->size; i++) {
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                t->data.f32_data[i] += (float)scalar;
                break;
            case TENSOR_FLOAT64:
                t->data.f64_data[i] += scalar;
                break;
            case TENSOR_INT32:
                t->data.i32_data[i] += (int32_t)scalar;
                break;
            case TENSOR_INT64:
                t->data.i64_data[i] += (int64_t)scalar;
                break;
            default:
                break;
        }
    }
}

/**
 * In-place scalar multiplication: t *= scalar
 * 
 * @param t        Tensor (modified in place)
 * @param scalar   Scalar value to multiply
 */
void tensor_imul_scalar(Tensor* t, double scalar) {
    if (t == NULL) return;
    
    for (size_t i = 0; i < t->size; i++) {
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                t->data.f32_data[i] *= (float)scalar;
                break;
            case TENSOR_FLOAT64:
                t->data.f64_data[i] *= scalar;
                break;
            case TENSOR_INT32:
                t->data.i32_data[i] *= (int32_t)scalar;
                break;
            case TENSOR_INT64:
                t->data.i64_data[i] *= (int64_t)scalar;
                break;
            default:
                break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * MATRIX OPERATIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Matrix multiplication: result = t1 @ t2
 * 
 * Performs standard matrix multiplication (dot product).
 * t1 must be (m x n), t2 must be (n x k), result is (m x k).
 * 
 * @param t1       First matrix (m x n)
 * @param t2       Second matrix (n x k)
 * @return         Result matrix (m x k)
 */
Tensor* tensor_matmul(Tensor* t1, Tensor* t2) {
    if (t1 == NULL || t2 == NULL) return NULL;
    if (t1->ndim != 2 || t2->ndim != 2) {
        fprintf(stderr, "[TENSOR] Matrix multiplication requires 2D tensors\n");
        return NULL;
    }
    
    int m = t1->shape[0];
    int n = t1->shape[1];
    int k = t2->shape[0];
    int p = t2->shape[1];
    
    if (n != k) {
        fprintf(stderr, "[TENSOR] Matrix dimension mismatch: %d x %d @ %d x %d\n",
                m, n, k, p);
        return NULL;
    }
    
    /* Create result tensor (m x p) */
    int result_shape[2] = {m, p};
    Tensor* result = tensor_new(TENSOR_FLOAT32, 2, result_shape);
    if (result == NULL) return NULL;
    
    /* Perform matrix multiplication */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < p; j++) {
            float sum = 0.0f;
            for (int k_idx = 0; k_idx < n; k_idx++) {
                /* t1[i, k_idx] * t2[k_idx, j] */
                sum += t1->data.f32_data[i * n + k_idx] * 
                       t2->data.f32_data[k_idx * p + j];
            }
            result->data.f32_data[i * p + j] = sum;
        }
    }
    
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 * REDUCTION OPERATIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Sum all elements in tensor
 * 
 * @param t        Input tensor
 * @return         Sum of all elements
 */
double tensor_sum(Tensor* t) {
    if (t == NULL) return 0.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < t->size; i++) {
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                sum += t->data.f32_data[i];
                break;
            case TENSOR_FLOAT64:
                sum += t->data.f64_data[i];
                break;
            case TENSOR_INT32:
                sum += t->data.i32_data[i];
                break;
            case TENSOR_INT64:
                sum += t->data.i64_data[i];
                break;
            default:
                break;
        }
    }
    return sum;
}

/**
 * Mean of all elements in tensor
 * 
 * @param t        Input tensor
 * @return         Mean of all elements
 */
double tensor_mean(Tensor* t) {
    if (t == NULL || t->size == 0) return 0.0;
    return tensor_sum(t) / (double)t->size;
}

/**
 * Standard deviation of all elements
 * 
 * Uses population standard deviation (divides by N, not N-1).
 * 
 * @param t        Input tensor
 * @return         Standard deviation
 */
double tensor_std(Tensor* t) {
    if (t == NULL || t->size == 0) return 0.0;
    
    double mean = tensor_mean(t);
    double variance = 0.0;
    
    for (size_t i = 0; i < t->size; i++) {
        double diff;
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                diff = t->data.f32_data[i] - mean;
                break;
            case TENSOR_FLOAT64:
                diff = t->data.f64_data[i] - mean;
                break;
            case TENSOR_INT32:
                diff = t->data.i32_data[i] - mean;
                break;
            case TENSOR_INT64:
                diff = t->data.i64_data[i] - mean;
                break;
            default:
                diff = 0.0;
        }
        variance += diff * diff;
    }
    
    variance /= (double)t->size;
    return sqrt(variance);
}

/**
 * Maximum value in tensor
 * 
 * @param t        Input tensor
 * @return         Maximum value
 */
double tensor_max(Tensor* t) {
    if (t == NULL || t->size == 0) return 0.0;
    
    double max_val = -INFINITY;
    for (size_t i = 0; i < t->size; i++) {
        double val;
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                val = t->data.f32_data[i];
                break;
            case TENSOR_FLOAT64:
                val = t->data.f64_data[i];
                break;
            case TENSOR_INT32:
                val = t->data.i32_data[i];
                break;
            case TENSOR_INT64:
                val = t->data.i64_data[i];
                break;
            default:
                val = -INFINITY;
        }
        if (val > max_val) max_val = val;
    }
    return max_val;
}

/**
 * Minimum value in tensor
 * 
 * @param t        Input tensor
 * @return         Minimum value
 */
double tensor_min(Tensor* t) {
    if (t == NULL || t->size == 0) return 0.0;
    
    double min_val = INFINITY;
    for (size_t i = 0; i < t->size; i++) {
        double val;
        switch (t->dtype) {
            case TENSOR_FLOAT32:
                val = t->data.f32_data[i];
                break;
            case TENSOR_FLOAT64:
                val = t->data.f64_data[i];
                break;
            case TENSOR_INT32:
                val = t->data.i32_data[i];
                break;
            case TENSOR_INT64:
                val = t->data.i64_data[i];
                break;
            default:
                val = INFINITY;
        }
        if (val < min_val) min_val = val;
    }
    return min_val;
}

/* ═══════════════════════════════════════════════════════════════════
 * ACTIVATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * ReLU activation: max(0, x)
 * 
 * Creates a new tensor with ReLU applied.
 * 
 * @param t        Input tensor
 * @return         Tensor with ReLU activation
 */
Tensor* tensor_relu(Tensor* t) {
    if (t == NULL) return NULL;
    
    Tensor* result = tensor_clone(t);
    if (result == NULL) return NULL;
    
    for (size_t i = 0; i < result->size; i++) {
        if (result->data.f32_data[i] < 0.0f) {
            result->data.f32_data[i] = 0.0f;
        }
    }
    
    return result;
}

/**
 * Sigmoid activation: 1 / (1 + exp(-x))
 * 
 * @param t        Input tensor
 * @return         Tensor with sigmoid activation
 */
Tensor* tensor_sigmoid(Tensor* t) {
    if (t == NULL) return NULL;
    
    Tensor* result = tensor_clone(t);
    if (result == NULL) return NULL;
    
    for (size_t i = 0; i < result->size; i++) {
        float x = result->data.f32_data[i];
        result->data.f32_data[i] = 1.0f / (1.0f + expf(-x));
    }
    
    return result;
}

/**
 * Tanh activation: tanh(x)
 * 
 * @param t        Input tensor
 * @return         Tensor with tanh activation
 */
Tensor* tensor_tanh(Tensor* t) {
    if (t == NULL) return NULL;
    
    Tensor* result = tensor_clone(t);
    if (result == NULL) return NULL;
    
    for (size_t i = 0; i < result->size; i++) {
        result->data.f32_data[i] = tanhf(result->data.f32_data[i]);
    }
    
    return result;
}

/**
 * Softmax activation: exp(x_i) / sum(exp(x))
 * 
 * Applies softmax across the last dimension.
 * 
 * @param t        Input tensor
 * @return         Tensor with softmax activation
 */
Tensor* tensor_softmax(Tensor* t) {
    if (t == NULL) return NULL;
    
    Tensor* result = tensor_clone(t);
    if (result == NULL) return NULL;
    
    if (t->ndim == 0) return result;
    
    /* Calculate softmax per "row" (last dimension) */
    int outer_size = 1;
    for (int i = 0; i < t->ndim - 1; i++) {
        outer_size *= t->shape[i];
    }
    int inner_size = t->shape[t->ndim - 1];
    
    for (int i = 0; i < outer_size; i++) {
        /* Find max for numerical stability */
        float max_val = -INFINITY;
        for (int j = 0; j < inner_size; j++) {
            float val = result->data.f32_data[i * inner_size + j];
            if (val > max_val) max_val = val;
        }
        
        /* Compute exponentials */
        float sum = 0.0f;
        for (int j = 0; j < inner_size; j++) {
            result->data.f32_data[i * inner_size + j] = 
                expf(result->data.f32_data[i * inner_size + j] - max_val);
            sum += result->data.f32_data[i * inner_size + j];
        }
        
        /* Normalize */
        for (int j = 0; j < inner_size; j++) {
            result->data.f32_data[i * inner_size + j] /= sum;
        }
    }
    
    return result;
}

/**
 * Compute gradient (placeholder for automatic differentiation)
 * 
 * This is a simplified gradient computation. In a full implementation,
 * this would use computational graphs for automatic differentiation.
 * 
 * @param loss     Loss tensor
 * @param target  Target tensor
 * @return        Gradient tensor
 */
Tensor* tensor_gradient(Tensor* loss, Tensor* target) {
    if (loss == NULL || target == NULL) return NULL;
    
    /* Simplified: return difference as gradient */
    Tensor* grad = tensor_clone(target);
    if (grad == NULL) return NULL;
    
    tensor_isub(grad, loss);
    
    return grad;
}

/**
 * Compute MSE loss gradient: d/dx (1/n * (x - y)^2) = 2/n * (x - y)
 */
Tensor* tensor_grad_mse(Tensor* pred, Tensor* target) {
    if (pred == NULL || target == NULL) return NULL;
    if (pred->size != target->size) return NULL;
    
    Tensor* grad = tensor_zeros(pred->dtype, pred->ndim, pred->shape);
    if (grad == NULL) return NULL;
    
    double scale = 2.0 / pred->size;
    for (size_t i = 0; i < pred->size; i++) {
        double diff = pred->data.f64_data[i] - target->data.f64_data[i];
        grad->data.f64_data[i] = scale * diff;
    }
    
    return grad;
}

/**
 * Compute cross-entropy loss gradient
 */
Tensor* tensor_grad_cross_entropy(Tensor* pred, Tensor* target) {
    if (pred == NULL || target == NULL) return NULL;
    
    Tensor* grad = tensor_clone(pred);
    if (grad == NULL) return NULL;
    
    /* d/dx (-y*log(x)) = -y/x */
    for (size_t i = 0; i < pred->size; i++) {
        if (target->data.f64_data[i] > 0.5) {
            if (pred->data.f64_data[i] > 1e-10) {
                grad->data.f64_data[i] = -1.0 / pred->data.f64_data[i];
            } else {
                grad->data.f64_data[i] = -1e10;
            }
        }
    }
    
    return grad;
}

/**
 * Compute ReLU gradient: d/dx max(0, x) = 1 if x > 0 else 0
 */
Tensor* tensor_grad_relu(Tensor* x, Tensor* grad_output) {
    if (x == NULL || grad_output == NULL) return NULL;
    
    Tensor* grad = tensor_clone(grad_output);
    if (grad == NULL) return NULL;
    
    for (size_t i = 0; i < x->size; i++) {
        if (x->data.f64_data[i] <= 0) {
            grad->data.f64_data[i] = 0;
        }
    }
    
    return grad;
}

/**
 * Compute Sigmoid gradient: d/dx sigmoid(x) = sigmoid(x) * (1 - sigmoid(x))
 */
Tensor* tensor_grad_sigmoid(Tensor* x, Tensor* grad_output) {
    if (x == NULL || grad_output == NULL) return NULL;
    
    Tensor* sigmoid_x = tensor_sigmoid(x);
    if (sigmoid_x == NULL) return NULL;
    
    Tensor* grad = tensor_clone(grad_output);
    if (grad == NULL) { tensor_free(sigmoid_x); return NULL; }
    
    /* d/dx sigmoid = sigmoid * (1 - sigmoid) */
    for (size_t i = 0; i < x->size; i++) {
        double s = sigmoid_x->data.f64_data[i];
        grad->data.f64_data[i] *= s * (1 - s);
    }
    
    tensor_free(sigmoid_x);
    return grad;
}

/**
 * Compute Tanh gradient: d/dx tanh(x) = 1 - tanh(x)^2
 */
Tensor* tensor_grad_tanh(Tensor* x, Tensor* grad_output) {
    if (x == NULL || grad_output == NULL) return NULL;
    
    Tensor* grad = tensor_clone(grad_output);
    if (grad == NULL) return NULL;
    
    /* d/dx tanh = 1 - tanh^2 */
    for (size_t i = 0; i < x->size; i++) {
        double t = tanh(x->data.f64_data[i]);
        grad->data.f64_data[i] *= (1 - t * t);
    }
    
    return grad;
}

/**
 * Compute matrix multiplication gradient
 * d/dA (A @ B) = gradient @ B^T
 * d/dB (A @ B) = A^T @ gradient
 */
void tensor_grad_matmul(Tensor* a, Tensor* b, Tensor* grad_output, 
                       Tensor** grad_a, Tensor** grad_b) {
    if (a == NULL || b == NULL || grad_output == NULL) return;
    
    /* grad_a = grad_output @ b^T */
    Tensor* b_t = tensor_transpose(b);
    *grad_a = tensor_matmul(grad_output, b_t);
    tensor_free(b_t);
    
    /* grad_b = a^T @ grad_output */
    Tensor* a_t = tensor_transpose(a);
    *grad_b = tensor_matmul(a_t, grad_output);
    tensor_free(a_t);
}

/**
 * Free a tensor and its data
 * 
 * @param t        Tensor to free
 */
void tensor_free(Tensor* t) {
    if (t == NULL) return;
    
    if (t->owns_data && t->data.f32_data != NULL) {
        free_tensor_data(t);
    }
    
    free(t->shape);
    free(t->strides);
    free(t);
}

/* ═══════════════════════════════════════════════════════════════════
 * NEURAL NETWORK MODEL
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Layer structure (opaque)
 */
struct Layer {
    LayerType type;
    void* layer_data;
    ActivationType activation;
    Tensor* (*forward)(void* layer_data, Tensor* input);
    Tensor* (*backward)(void* layer_data, Tensor* grad_output, Tensor* input);
    void (*free)(void* layer_data);
};

/**
 * Dense layer data
 */
typedef struct {
    Tensor* weights;
    Tensor* biases;
    Tensor* input;
    Tensor* output;
    Tensor* weight_grad;
    Tensor* bias_grad;
} DenseLayerData;

typedef struct {
    int filters;
    int kernel_size;
    int stride;
    int padding;
    int input_width;
    int output_width;
    Tensor* weights;      /* shape: [filters, kernel_size] */
    Tensor* biases;       /* shape: [filters] */
    Tensor* input;
    Tensor* output;
    Tensor* weight_grad;
    Tensor* bias_grad;
} Conv1DLayerData;

typedef struct {
    float rate;
    bool training;
    Tensor* mask;
} DropoutLayerData;

typedef struct {
    int pool_size;
    int stride;
    int input_width;
    int output_width;
    int* max_indices;     /* shape: [batch, output_width] */
    int max_indices_count;
} MaxPool1DLayerData;

static void dense_clear_cached_io(DenseLayerData* data) {
    if (data == NULL) return;
    tensor_free(data->input);
    tensor_free(data->output);
    data->input = NULL;
    data->output = NULL;
}

static Tensor* conv1d_forward(void* layer_data, Tensor* input) {
    Conv1DLayerData* data = (Conv1DLayerData*)layer_data;
    if (data == NULL || input == NULL || input->dtype != TENSOR_FLOAT32 || input->ndim != 2) return NULL;

    tensor_free(data->input);
    tensor_free(data->output);
    data->input = tensor_clone(input);

    int batch = input->shape[0];
    int in_w = input->shape[1];
    if (in_w <= 0 || data->kernel_size <= 0 || data->filters <= 0 || data->stride <= 0) return NULL;

    int out_w = (in_w + 2 * data->padding - data->kernel_size) / data->stride + 1;
    if (out_w <= 0) return NULL;

    if (data->weights == NULL || data->biases == NULL || data->input_width != in_w || data->output_width != out_w) {
        int weight_shape[2] = {data->filters, data->kernel_size};
        int bias_shape[1] = {data->filters};
        tensor_free(data->weights);
        tensor_free(data->biases);
        data->weights = tensor_randn(2, weight_shape, 0.0, sqrt(2.0 / (double)(data->kernel_size + data->filters)));
        data->biases = tensor_zeros(TENSOR_FLOAT32, 1, bias_shape);
        data->input_width = in_w;
        data->output_width = out_w;
    }

    {
        int out_shape[2] = {batch, out_w * data->filters};
        Tensor* out = tensor_zeros(TENSOR_FLOAT32, 2, out_shape);
        if (out == NULL) return NULL;

        for (int b = 0; b < batch; b++) {
            for (int f = 0; f < data->filters; f++) {
                float bias = data->biases->data.f32_data[f];
                for (int ow = 0; ow < out_w; ow++) {
                    float sum = bias;
                    for (int k = 0; k < data->kernel_size; k++) {
                        int in_idx = ow * data->stride + k - data->padding;
                        if (in_idx >= 0 && in_idx < in_w) {
                            float x = input->data.f32_data[b * in_w + in_idx];
                            float w = data->weights->data.f32_data[f * data->kernel_size + k];
                            sum += x * w;
                        }
                    }
                    out->data.f32_data[b * (out_w * data->filters) + f * out_w + ow] = sum;
                }
            }
        }

        data->output = out;
        return out;
    }
}

static Tensor* conv1d_backward(void* layer_data, Tensor* grad_output, Tensor* input) {
    Conv1DLayerData* data = (Conv1DLayerData*)layer_data;
    UNUSED(input);
    if (data == NULL || grad_output == NULL || data->input == NULL || data->weights == NULL) return NULL;
    if (grad_output->dtype != TENSOR_FLOAT32 || grad_output->ndim != 2) return NULL;

    int batch = data->input->shape[0];
    int in_w = data->input->shape[1];
    int out_w = data->output_width;
    if (batch <= 0 || in_w <= 0 || out_w <= 0) return NULL;

    {
        int grad_in_shape[2] = {batch, in_w};
        int grad_w_shape[2] = {data->filters, data->kernel_size};
        int grad_b_shape[1] = {data->filters};
        Tensor* grad_input = tensor_zeros(TENSOR_FLOAT32, 2, grad_in_shape);
        Tensor* grad_w = tensor_zeros(TENSOR_FLOAT32, 2, grad_w_shape);
        Tensor* grad_b = tensor_zeros(TENSOR_FLOAT32, 1, grad_b_shape);
        if (grad_input == NULL || grad_w == NULL || grad_b == NULL) {
            tensor_free(grad_input);
            tensor_free(grad_w);
            tensor_free(grad_b);
            return NULL;
        }

        for (int b = 0; b < batch; b++) {
            for (int f = 0; f < data->filters; f++) {
                for (int ow = 0; ow < out_w; ow++) {
                    float go = grad_output->data.f32_data[b * (out_w * data->filters) + f * out_w + ow];
                    grad_b->data.f32_data[f] += go;
                    for (int k = 0; k < data->kernel_size; k++) {
                        int in_idx = ow * data->stride + k - data->padding;
                        if (in_idx >= 0 && in_idx < in_w) {
                            float x = data->input->data.f32_data[b * in_w + in_idx];
                            float w = data->weights->data.f32_data[f * data->kernel_size + k];
                            grad_w->data.f32_data[f * data->kernel_size + k] += x * go;
                            grad_input->data.f32_data[b * in_w + in_idx] += w * go;
                        }
                    }
                }
            }
        }

        if (batch > 0) {
            float inv_batch = 1.0f / (float)batch;
            tensor_imul_scalar(grad_w, inv_batch);
            tensor_imul_scalar(grad_b, inv_batch);
        }

        tensor_free(data->weight_grad);
        tensor_free(data->bias_grad);
        data->weight_grad = grad_w;
        data->bias_grad = grad_b;
        return grad_input;
    }
}

static void conv1d_free(void* layer_data) {
    Conv1DLayerData* data = (Conv1DLayerData*)layer_data;
    if (data == NULL) return;
    tensor_free(data->weights);
    tensor_free(data->biases);
    tensor_free(data->input);
    tensor_free(data->output);
    tensor_free(data->weight_grad);
    tensor_free(data->bias_grad);
    free(data);
}

static Tensor* dropout_forward(void* layer_data, Tensor* input) {
    DropoutLayerData* data = (DropoutLayerData*)layer_data;
    if (data == NULL || input == NULL) return NULL;

    tensor_free(data->mask);
    data->mask = NULL;

    if (!data->training || data->rate <= 0.0f) {
        return tensor_clone(input);
    }

    {
        Tensor* out = tensor_clone(input);
        Tensor* mask = tensor_zeros(TENSOR_FLOAT32, input->ndim, input->shape);
        float keep_prob = 1.0f - data->rate;
        if (out == NULL || mask == NULL || keep_prob <= 0.0f) {
            tensor_free(out);
            tensor_free(mask);
            return tensor_clone(input);
        }

        for (size_t i = 0; i < input->size; i++) {
            float keep = (((float)rand() / (float)RAND_MAX) < keep_prob) ? (1.0f / keep_prob) : 0.0f;
            mask->data.f32_data[i] = keep;
            out->data.f32_data[i] *= keep;
        }

        data->mask = mask;
        return out;
    }
}

static Tensor* dropout_backward(void* layer_data, Tensor* grad_output, Tensor* input) {
    DropoutLayerData* data = (DropoutLayerData*)layer_data;
    UNUSED(input);
    if (data == NULL || grad_output == NULL) return NULL;
    if (data->mask == NULL || !tensor_same_shape(data->mask, grad_output)) return tensor_clone(grad_output);

    {
        Tensor* grad = tensor_clone(grad_output);
        if (grad == NULL) return NULL;
        tensor_imul(grad, data->mask);
        return grad;
    }
}

static void dropout_free(void* layer_data) {
    DropoutLayerData* data = (DropoutLayerData*)layer_data;
    if (data == NULL) return;
    tensor_free(data->mask);
    free(data);
}

static Tensor* maxpool1d_forward(void* layer_data, Tensor* input) {
    MaxPool1DLayerData* data = (MaxPool1DLayerData*)layer_data;
    if (data == NULL || input == NULL || input->dtype != TENSOR_FLOAT32 || input->ndim != 2) return NULL;

    int batch = input->shape[0];
    int in_w = input->shape[1];
    if (in_w <= 0 || data->pool_size <= 0 || data->stride <= 0) return NULL;
    int out_w = (in_w - data->pool_size) / data->stride + 1;
    if (out_w <= 0) return NULL;

    {
        int out_shape[2] = {batch, out_w};
        Tensor* out = tensor_zeros(TENSOR_FLOAT32, 2, out_shape);
        if (out == NULL) return NULL;

        free(data->max_indices);
        data->max_indices = (int*)malloc(sizeof(int) * batch * out_w);
        data->max_indices_count = batch * out_w;
        data->input_width = in_w;
        data->output_width = out_w;
        if (data->max_indices == NULL) {
            tensor_free(out);
            return NULL;
        }

        for (int b = 0; b < batch; b++) {
            for (int ow = 0; ow < out_w; ow++) {
                int start = ow * data->stride;
                float max_val = -INFINITY;
                int max_idx = start;
                for (int k = 0; k < data->pool_size; k++) {
                    int idx = start + k;
                    if (idx >= 0 && idx < in_w) {
                        float v = input->data.f32_data[b * in_w + idx];
                        if (v > max_val) {
                            max_val = v;
                            max_idx = idx;
                        }
                    }
                }
                out->data.f32_data[b * out_w + ow] = max_val;
                data->max_indices[b * out_w + ow] = max_idx;
            }
        }

        return out;
    }
}

static Tensor* maxpool1d_backward(void* layer_data, Tensor* grad_output, Tensor* input) {
    MaxPool1DLayerData* data = (MaxPool1DLayerData*)layer_data;
    UNUSED(input);
    if (data == NULL || grad_output == NULL || grad_output->dtype != TENSOR_FLOAT32 || grad_output->ndim != 2) return NULL;
    if (data->max_indices == NULL || data->input_width <= 0 || data->output_width <= 0) return NULL;

    {
        int batch = grad_output->shape[0];
        int grad_in_shape[2] = {batch, data->input_width};
        Tensor* grad_input = tensor_zeros(TENSOR_FLOAT32, 2, grad_in_shape);
        if (grad_input == NULL) return NULL;

        for (int b = 0; b < batch; b++) {
            for (int ow = 0; ow < data->output_width; ow++) {
                int src_idx = b * data->output_width + ow;
                int dst = data->max_indices[src_idx];
                if (dst >= 0 && dst < data->input_width) {
                    grad_input->data.f32_data[b * data->input_width + dst] += grad_output->data.f32_data[src_idx];
                }
            }
        }

        return grad_input;
    }
}

static void maxpool1d_free(void* layer_data) {
    MaxPool1DLayerData* data = (MaxPool1DLayerData*)layer_data;
    if (data == NULL) return;
    free(data->max_indices);
    free(data);
}

/**
 * Forward pass for dense layer
 */
static Tensor* dense_forward(void* layer_data, Tensor* input) {
    DenseLayerData* data = (DenseLayerData*)layer_data;
    if (data == NULL || input == NULL) return NULL;

    dense_clear_cached_io(data);
    
    /* Cache input for backward pass */
    data->input = tensor_clone(input);
    
    /* output = input @ weights + biases */
    Tensor* weighted = tensor_matmul(input, data->weights);
    if (weighted == NULL) return NULL;
    
    /* Add biases (broadcast) */
    if (data->biases != NULL) {
        tensor_iadd(weighted, data->biases);
    }
    
    data->output = weighted;
    return weighted;
}

/**
 * Backward pass for dense layer
 */
static Tensor* dense_backward(void* layer_data, Tensor* grad_output, Tensor* input) {
    DenseLayerData* data = (DenseLayerData*)layer_data;
    if (data == NULL || grad_output == NULL) return NULL;
    if (input == NULL) input = data->input;
    if (input == NULL) return NULL;
    if (input->ndim != 2 || grad_output->ndim != 2) return NULL;
    
    /* Compute weight gradients: input.T @ grad_output */
    Tensor* input_T = tensor_transpose(input);
    if (input_T == NULL) return NULL;
    Tensor* weight_grad = tensor_matmul(input_T, grad_output);
    tensor_free(input_T);
    if (weight_grad == NULL) return NULL;
    
    /* Compute bias gradients: sum(grad_output, axis=0) */
    int batch = grad_output->shape[0];
    int units = grad_output->shape[1];
    int bias_shape[1] = {units};
    Tensor* bias_grad = tensor_zeros(TENSOR_FLOAT32, 1, bias_shape);
    if (bias_grad == NULL) {
        tensor_free(weight_grad);
        return NULL;
    }

    for (int b = 0; b < batch; b++) {
        for (int u = 0; u < units; u++) {
            bias_grad->data.f32_data[u] += grad_output->data.f32_data[b * units + u];
        }
    }

    if (batch > 0) {
        float inv_batch = 1.0f / (float)batch;
        tensor_imul_scalar(weight_grad, inv_batch);
        tensor_imul_scalar(bias_grad, inv_batch);
    }

    /* Compute input gradients for upstream layer: grad_output @ weights.T */
    Tensor* weights_T = tensor_transpose(data->weights);
    Tensor* grad_input = NULL;
    if (weights_T != NULL) {
        grad_input = tensor_matmul(grad_output, weights_T);
        tensor_free(weights_T);
    }

    if (data->weight_grad != NULL) tensor_free(data->weight_grad);
    if (data->bias_grad != NULL) tensor_free(data->bias_grad);
    
    data->weight_grad = weight_grad;
    data->bias_grad = bias_grad;
    return grad_input;
}

/**
 * Free dense layer
 */
static void dense_free(void* layer_data) {
    DenseLayerData* data = (DenseLayerData*)layer_data;
    if (data == NULL) return;
    
    tensor_free(data->weights);
    tensor_free(data->biases);
    tensor_free(data->input);
    tensor_free(data->output);
    tensor_free(data->weight_grad);
    tensor_free(data->bias_grad);
    free(data);
}

/**
 * Create a new neural network model
 * 
 * @param name     Name of the model
 * @return         New model instance
 */
NNModel* nn_model_new(const char* name) {
    NNModel* model = (NNModel*)malloc(sizeof(NNModel));
    if (model == NULL) return NULL;
    
    model->name = safe_strdup(name);
    model->layers = NULL;
    model->layer_count = 0;
    model->layer_capacity = 0;
    model->input = NULL;
    model->output = NULL;
    model->loss = NULL;
    model->learning_rate = 0.001;
    model->training = true;
    
    return model;
}

/**
 * Add a dense (fully connected) layer to the model
 * 
 * @param model         Model to add layer to
 * @param units         Number of output units
 * @param activation   Activation function
 */
void nn_model_add_dense(NNModel* model, int units, ActivationType activation, int input_size_override) {
    if (model == NULL) return;
    
    /* Grow layers array if needed */
    if (model->layer_count >= model->layer_capacity) {
        int new_capacity = model->layer_capacity == 0 ? 4 : model->layer_capacity * 2;
        model->layers = (Layer**)realloc(model->layers, new_capacity * sizeof(Layer*));
        model->layer_capacity = new_capacity;
    }
    
    /* Create layer */
    Layer* layer = (Layer*)malloc(sizeof(Layer));
    DenseLayerData* data = (DenseLayerData*)malloc(sizeof(DenseLayerData));
    
    /* Get input size from previous layer or use explicit override */
    int input_size = (input_size_override > 0) ? input_size_override : 784;  /* Default for MNIST */
    if (model->layer_count > 0) {
        Layer* prev = model->layers[model->layer_count - 1];
        if (prev != NULL && prev->type == LAYER_DENSE && prev->layer_data != NULL) {
            DenseLayerData* prev_data = (DenseLayerData*)prev->layer_data;
            if (prev_data->weights != NULL && prev_data->weights->ndim == 2) {
                input_size = prev_data->weights->shape[1];
            }
        }
    }
    
    /* Initialize Xavier/He weights */
    double scale = sqrt(2.0 / (input_size + units));
    int weight_shape[2] = {input_size, units};
    data->weights = tensor_randn(2, weight_shape, 0.0, scale);
    
    int bias_shape[1] = {units};
    data->biases = tensor_zeros(TENSOR_FLOAT32, 1, bias_shape);
    
    data->input = NULL;
    data->output = NULL;
    data->weight_grad = NULL;
    data->bias_grad = NULL;
    
    layer->type = LAYER_DENSE;
    layer->layer_data = data;
    layer->activation = activation;
    layer->forward = dense_forward;
    layer->backward = dense_backward;
    layer->free = dense_free;
    
    model->layers[model->layer_count++] = layer;
}

/**
 * Add a convolutional 2D layer
 * 
 * @param model         Model to add layer to
 * @param filters       Number of output filters
 * @param kernel_size   Size of the convolution kernel
 * @param stride        Convolution stride
 * @param padding       Padding size
 * @param activation   Activation function
 */
void nn_model_add_conv2d(NNModel* model, int filters, int kernel_size,
                        int stride, int padding, ActivationType activation) {
    if (model == NULL || filters <= 0 || kernel_size <= 0 || stride <= 0) return;

    if (model->layer_count >= model->layer_capacity) {
        int new_capacity = model->layer_capacity == 0 ? 4 : model->layer_capacity * 2;
        Layer** grown = (Layer**)realloc(model->layers, new_capacity * sizeof(Layer*));
        if (grown == NULL) return;
        model->layers = grown;
        model->layer_capacity = new_capacity;
    }

    {
        Layer* layer = (Layer*)calloc(1, sizeof(Layer));
        Conv1DLayerData* data = (Conv1DLayerData*)calloc(1, sizeof(Conv1DLayerData));
        if (layer == NULL || data == NULL) {
            free(layer);
            free(data);
            return;
        }

        data->filters = filters;
        data->kernel_size = kernel_size;
        data->stride = stride;
        data->padding = padding;

        layer->type = LAYER_CONV2D;
        layer->layer_data = data;
        layer->activation = activation;
        layer->forward = conv1d_forward;
        layer->backward = conv1d_backward;
        layer->free = conv1d_free;

        model->layers[model->layer_count++] = layer;
    }
}

/**
 * Add a dropout layer
 * 
 * @param model         Model to add layer to
 * @param rate          Dropout rate (probability of dropping)
 */
void nn_model_add_dropout(NNModel* model, double rate) {
    if (model == NULL || rate < 0.0 || rate >= 1.0) return;

    if (model->layer_count >= model->layer_capacity) {
        int new_capacity = model->layer_capacity == 0 ? 4 : model->layer_capacity * 2;
        Layer** grown = (Layer**)realloc(model->layers, new_capacity * sizeof(Layer*));
        if (grown == NULL) return;
        model->layers = grown;
        model->layer_capacity = new_capacity;
    }

    {
        Layer* layer = (Layer*)calloc(1, sizeof(Layer));
        DropoutLayerData* data = (DropoutLayerData*)calloc(1, sizeof(DropoutLayerData));
        if (layer == NULL || data == NULL) {
            free(layer);
            free(data);
            return;
        }

        data->rate = (float)rate;
        data->training = true;

        layer->type = LAYER_DROPOUT;
        layer->layer_data = data;
        layer->activation = ACTIVATION_NONE;
        layer->forward = dropout_forward;
        layer->backward = dropout_backward;
        layer->free = dropout_free;

        model->layers[model->layer_count++] = layer;
    }
}

/**
 * Add a max pooling 2D layer
 * 
 * @param model         Model to add layer to
 * @param pool_size     Size of the pooling window
 * @param stride        Pooling stride
 */
void nn_model_add_maxpool2d(NNModel* model, int pool_size, int stride) {
    if (model == NULL || pool_size <= 0 || stride <= 0) return;

    if (model->layer_count >= model->layer_capacity) {
        int new_capacity = model->layer_capacity == 0 ? 4 : model->layer_capacity * 2;
        Layer** grown = (Layer**)realloc(model->layers, new_capacity * sizeof(Layer*));
        if (grown == NULL) return;
        model->layers = grown;
        model->layer_capacity = new_capacity;
    }

    {
        Layer* layer = (Layer*)calloc(1, sizeof(Layer));
        MaxPool1DLayerData* data = (MaxPool1DLayerData*)calloc(1, sizeof(MaxPool1DLayerData));
        if (layer == NULL || data == NULL) {
            free(layer);
            free(data);
            return;
        }

        data->pool_size = pool_size;
        data->stride = stride;

        layer->type = LAYER_MAXPOOL2D;
        layer->layer_data = data;
        layer->activation = ACTIVATION_NONE;
        layer->forward = maxpool1d_forward;
        layer->backward = maxpool1d_backward;
        layer->free = maxpool1d_free;

        model->layers[model->layer_count++] = layer;
    }
}

/**
 * Forward pass through the model
 * 
 * @param model         Neural network model
 * @param input         Input tensor
 * @return             Output tensor
 */
Tensor* nn_model_forward(NNModel* model, Tensor* input) {
    if (model == NULL || input == NULL) return NULL;
    
    Tensor* current = input;
    model->input = input;
    
    for (int i = 0; i < model->layer_count; i++) {
        Layer* layer = model->layers[i];
        if (layer == NULL) continue;

        if (layer->type == LAYER_DROPOUT && layer->layer_data != NULL) {
            DropoutLayerData* dropout = (DropoutLayerData*)layer->layer_data;
            dropout->training = model->training;
        }
        
        /* Forward pass */
        Tensor* layer_output = layer->forward(layer->layer_data, current);
        if (layer_output == NULL) return NULL;
        
        /* Apply activation */
        switch (layer->activation) {
            case ACTIVATION_RELU:
                layer_output = tensor_relu(layer_output);
                break;
            case ACTIVATION_SIGMOID:
                layer_output = tensor_sigmoid(layer_output);
                break;
            case ACTIVATION_TANH:
                layer_output = tensor_tanh(layer_output);
                break;
            case ACTIVATION_SOFTMAX:
                layer_output = tensor_softmax(layer_output);
                break;
            case ACTIVATION_NONE:
            default:
                break;
        }
        
        current = layer_output;
    }
    
    model->output = current;
    return current;
}

/**
 * Backward pass through the model
 * 
 * @param model         Neural network model
 * @param loss          Loss gradient
 */
void nn_model_backward(NNModel* model, Tensor* loss) {
    if (model == NULL || loss == NULL) return;

    Tensor* grad = tensor_clone(loss);
    if (grad == NULL) return;

    for (int i = model->layer_count - 1; i >= 0; i--) {
        Layer* layer = model->layers[i];
        if (layer == NULL || layer->layer_data == NULL) continue;

        if (layer->type == LAYER_DENSE) {
            DenseLayerData* data = (DenseLayerData*)layer->layer_data;
            if (data->output == NULL) continue;

            if (layer->activation == ACTIVATION_RELU) {
                for (size_t j = 0; j < grad->size && j < data->output->size; j++) {
                    if (data->output->data.f32_data[j] <= 0.0f) grad->data.f32_data[j] = 0.0f;
                }
            } else if (layer->activation == ACTIVATION_SIGMOID) {
                for (size_t j = 0; j < grad->size && j < data->output->size; j++) {
                    float z = data->output->data.f32_data[j];
                    float s = 1.0f / (1.0f + expf(-z));
                    grad->data.f32_data[j] *= s * (1.0f - s);
                }
            } else if (layer->activation == ACTIVATION_TANH) {
                for (size_t j = 0; j < grad->size && j < data->output->size; j++) {
                    float z = data->output->data.f32_data[j];
                    float t = tanhf(z);
                    grad->data.f32_data[j] *= (1.0f - t * t);
                }
            }
        }

        Tensor* prev_grad = layer->backward(layer->layer_data, grad, NULL);
        tensor_free(grad);
        grad = prev_grad;
        if (grad == NULL) break;
    }

    tensor_free(grad);
}

/**
 * Update model weights using gradients
 * 
 * @param model         Neural network model
 */
void nn_model_update(NNModel* model) {
    if (model == NULL) return;

    for (int i = 0; i < model->layer_count; i++) {
        Layer* layer = model->layers[i];
        if (layer == NULL || layer->layer_data == NULL) continue;

        if (layer->type == LAYER_DENSE) {
            DenseLayerData* data = (DenseLayerData*)layer->layer_data;
            if (data->weights != NULL && data->weight_grad != NULL) {
                int count = (int)(data->weights->size < data->weight_grad->size ? data->weights->size : data->weight_grad->size);
                for (int j = 0; j < count; j++) {
                    data->weights->data.f32_data[j] -= (float)(model->learning_rate) * data->weight_grad->data.f32_data[j];
                }
            }

            if (data->biases != NULL && data->bias_grad != NULL) {
                int count = (int)(data->biases->size < data->bias_grad->size ? data->biases->size : data->bias_grad->size);
                for (int j = 0; j < count; j++) {
                    data->biases->data.f32_data[j] -= (float)(model->learning_rate) * data->bias_grad->data.f32_data[j];
                }
            }

            tensor_free(data->weight_grad);
            tensor_free(data->bias_grad);
            data->weight_grad = NULL;
            data->bias_grad = NULL;
            dense_clear_cached_io(data);
        } else if (layer->type == LAYER_CONV2D) {
            Conv1DLayerData* data = (Conv1DLayerData*)layer->layer_data;
            if (data->weights != NULL && data->weight_grad != NULL) {
                int count = (int)(data->weights->size < data->weight_grad->size ? data->weights->size : data->weight_grad->size);
                for (int j = 0; j < count; j++) {
                    data->weights->data.f32_data[j] -= (float)(model->learning_rate) * data->weight_grad->data.f32_data[j];
                }
            }
            if (data->biases != NULL && data->bias_grad != NULL) {
                int count = (int)(data->biases->size < data->bias_grad->size ? data->biases->size : data->bias_grad->size);
                for (int j = 0; j < count; j++) {
                    data->biases->data.f32_data[j] -= (float)(model->learning_rate) * data->bias_grad->data.f32_data[j];
                }
            }
            tensor_free(data->weight_grad);
            tensor_free(data->bias_grad);
            data->weight_grad = NULL;
            data->bias_grad = NULL;
            tensor_free(data->input);
            tensor_free(data->output);
            data->input = NULL;
            data->output = NULL;
        }
    }
}

/**
 * Train the model for one epoch
 * 
 * @param model         Neural network model
 * @param X             Training inputs
 * @param y             Training targets
 * @param batch_size    Batch size
 */
double nn_model_train_with_loss(NNModel* model, Tensor* X, Tensor* y, int batch_size, LossType loss_type) {
    if (model == NULL || X == NULL || y == NULL) return 0.0;

    model->training = true;

    if (X->ndim != 2 || y->ndim != 2) return 0.0;
    if (X->shape[0] != y->shape[0]) return 0.0;

    int total = X->shape[0];
    int in_dim = X->shape[1];
    int out_dim = y->shape[1];
    if (batch_size <= 0 || batch_size > total) batch_size = total;

    int x_shape[2] = {batch_size, in_dim};
    int y_shape[2] = {batch_size, out_dim};
    Tensor* xb = tensor_new(TENSOR_FLOAT32, 2, x_shape);
    Tensor* yb = tensor_new(TENSOR_FLOAT32, 2, y_shape);
    if (xb == NULL || yb == NULL) {
        tensor_free(xb);
        tensor_free(yb);
        return 0.0;
    }

    double total_loss = 0.0;
    int total_seen = 0;

    int start = 0;
    while (start < total) {
        int current_batch = batch_size;
        if (start + current_batch > total) current_batch = total - start;

        for (int r = 0; r < current_batch; r++) {
            memcpy(&xb->data.f32_data[r * in_dim], &X->data.f32_data[(start + r) * in_dim], (size_t)in_dim * sizeof(float));
            memcpy(&yb->data.f32_data[r * out_dim], &y->data.f32_data[(start + r) * out_dim], (size_t)out_dim * sizeof(float));
        }

        if (current_batch < batch_size) {
            memset(&xb->data.f32_data[current_batch * in_dim], 0, (size_t)(batch_size - current_batch) * (size_t)in_dim * sizeof(float));
            memset(&yb->data.f32_data[current_batch * out_dim], 0, (size_t)(batch_size - current_batch) * (size_t)out_dim * sizeof(float));
        }

        Tensor* pred = nn_model_forward(model, xb);
        if (pred == NULL || pred->ndim != 2 || pred->shape[0] != batch_size || pred->shape[1] != out_dim) {
            start += current_batch;
            continue;
        }

        int grad_shape[2] = {batch_size, out_dim};
        Tensor* grad = tensor_new(TENSOR_FLOAT32, 2, grad_shape);
        if (grad == NULL) {
            start += current_batch;
            continue;
        }

        if (loss_type == LOSS_CROSS_ENTROPY) {
            float inv_batch = (current_batch > 0) ? (1.0f / (float)current_batch) : 0.0f;
            for (int i = 0; i < current_batch * out_dim; i++) {
                double pred_val = (double)pred->data.f32_data[i];
                double target_val = (double)yb->data.f32_data[i];
                double clamped = pred_val < 1e-7 ? 1e-7 : (pred_val > 1.0 - 1e-7 ? 1.0 - 1e-7 : pred_val);
                total_loss += -target_val * log(clamped);
                grad->data.f32_data[i] = (float)((pred_val - target_val) * inv_batch);
            }
            total_seen += current_batch;
        } else {
            float scale = (current_batch > 0) ? (2.0f / (float)current_batch) : 0.0f;
            for (int i = 0; i < current_batch * out_dim; i++) {
                double diff = (double)pred->data.f32_data[i] - (double)yb->data.f32_data[i];
                total_loss += diff * diff;
                grad->data.f32_data[i] = (float)(diff * scale);
            }
            total_seen += current_batch * out_dim;
        }

        for (int i = current_batch * out_dim; i < batch_size * out_dim; i++) {
            grad->data.f32_data[i] = 0.0f;
        }

        nn_model_backward(model, grad);
        nn_model_update(model);
        tensor_free(grad);

        start += current_batch;
    }

    tensor_free(xb);
    tensor_free(yb);
    return total_seen > 0 ? (total_loss / (double)total_seen) : 0.0;
}

double nn_model_train(NNModel* model, Tensor* X, Tensor* y, int batch_size) {
    return nn_model_train_with_loss(model, X, y, batch_size, LOSS_MSE);
}

/**
 * Make predictions with the model
 * 
 * @param model         Neural network model
 * @param input         Input tensor
 * @return             Prediction tensor
 */
Tensor* nn_model_predict(NNModel* model, Tensor* input) {
    if (model == NULL || input == NULL) return NULL;
    
    model->training = false;
    return nn_model_forward(model, input);
}

void nn_model_set_learning_rate(NNModel* model, double learning_rate) {
    if (model == NULL) return;
    if (learning_rate <= 0.0) return;
    model->learning_rate = learning_rate;
}

double nn_model_get_learning_rate(NNModel* model) {
    if (model == NULL) return 0.0;
    return model->learning_rate;
}

/**
 * Save model to file
 * 
 * @param model         Neural network model
 * @param filepath      Output file path
 * @return             True if successful
 */
bool nn_model_save(NNModel* model, const char* filepath) {
    if (model == NULL || filepath == NULL) return false;
    
    FILE* f = fopen(filepath, "wb");
    if (f == NULL) {
        fprintf(stderr, "[NN] Cannot open file for saving: %s\n", filepath);
        return false;
    }
    
    {
        const char magic[8] = {'V','X','M','N','N','1','\0','\0'};
        uint32_t version = 1;
        uint32_t name_len = (uint32_t)strlen(model->name);

        if (!write_exact(magic, sizeof(char), 8, f) ||
            !write_exact(&version, sizeof(uint32_t), 1, f) ||
            !write_exact(&name_len, sizeof(uint32_t), 1, f) ||
            !write_exact(model->name, sizeof(char), name_len, f) ||
            !write_exact(&model->layer_count, sizeof(int), 1, f) ||
            !write_exact(&model->learning_rate, sizeof(double), 1, f)) {
            fclose(f);
            return false;
        }

        for (int i = 0; i < model->layer_count; i++) {
            Layer* layer = model->layers[i];
            DenseLayerData* data = (layer != NULL) ? (DenseLayerData*)layer->layer_data : NULL;
            int layer_type = (layer != NULL) ? (int)layer->type : -1;
            int activation = (layer != NULL) ? (int)layer->activation : -1;
            int input_size = (data != NULL && data->weights != NULL && data->weights->ndim == 2) ? data->weights->shape[0] : 0;
            int units = (data != NULL && data->weights != NULL && data->weights->ndim == 2) ? data->weights->shape[1] : 0;
            int use_bias = (data != NULL && data->biases != NULL) ? 1 : 0;

            if (layer_type != LAYER_DENSE || input_size <= 0 || units <= 0 ||
                !write_exact(&layer_type, sizeof(int), 1, f) ||
                !write_exact(&activation, sizeof(int), 1, f) ||
                !write_exact(&input_size, sizeof(int), 1, f) ||
                !write_exact(&units, sizeof(int), 1, f) ||
                !write_exact(&use_bias, sizeof(int), 1, f) ||
                !write_exact(data->weights->data.f32_data, sizeof(float), (size_t)input_size * (size_t)units, f)) {
                fclose(f);
                return false;
            }

            if (use_bias && !write_exact(data->biases->data.f32_data, sizeof(float), (size_t)units, f)) {
                fclose(f);
                return false;
            }
        }
    }
    
    fclose(f);
    return true;
}

/**
 * Load model from file
 * 
 * @param filepath      Input file path
 * @return            Loaded model or NULL
 */
NNModel* nn_model_load(const char* filepath) {
    if (filepath == NULL) return NULL;
    
    FILE* f = fopen(filepath, "rb");
    if (f == NULL) {
        fprintf(stderr, "[NN] Cannot open file for loading: %s\n", filepath);
        return NULL;
    }
    
    {
        char magic[8] = {0};
        uint32_t version = 0;
        uint32_t name_len = 0;
        int layer_count = 0;
        char* name = NULL;
        NNModel* model = NULL;

        if (!read_exact(magic, sizeof(char), 8, f) || memcmp(magic, "VXMNN1\0\0", 8) != 0 ||
            !read_exact(&version, sizeof(uint32_t), 1, f) || version != 1 ||
            !read_exact(&name_len, sizeof(uint32_t), 1, f) || name_len == 0 || name_len > 4096) {
            fclose(f);
            return NULL;
        }

        name = (char*)malloc((size_t)name_len + 1);
        if (name == NULL || !read_exact(name, sizeof(char), name_len, f)) {
            free(name);
            fclose(f);
            return NULL;
        }
        name[name_len] = '\0';

        model = nn_model_new(name);
        free(name);
        if (model == NULL ||
            !read_exact(&layer_count, sizeof(int), 1, f) ||
            !read_exact(&model->learning_rate, sizeof(double), 1, f)) {
            nn_model_free(model);
            fclose(f);
            return NULL;
        }

        for (int i = 0; i < layer_count; i++) {
            int layer_type = 0;
            int activation = 0;
            int input_size = 0;
            int units = 0;
            int use_bias = 0;

            if (!read_exact(&layer_type, sizeof(int), 1, f) ||
                !read_exact(&activation, sizeof(int), 1, f) ||
                !read_exact(&input_size, sizeof(int), 1, f) ||
                !read_exact(&units, sizeof(int), 1, f) ||
                !read_exact(&use_bias, sizeof(int), 1, f) ||
                layer_type != LAYER_DENSE || input_size <= 0 || units <= 0) {
                nn_model_free(model);
                fclose(f);
                return NULL;
            }

            nn_model_add_dense(model, units, (ActivationType)activation, input_size);
            if (model->layer_count <= 0 || model->layers[model->layer_count - 1] == NULL || model->layers[model->layer_count - 1]->layer_data == NULL) {
                nn_model_free(model);
                fclose(f);
                return NULL;
            }

            {
                DenseLayerData* data = (DenseLayerData*)model->layers[model->layer_count - 1]->layer_data;
                if (!read_exact(data->weights->data.f32_data, sizeof(float), (size_t)input_size * (size_t)units, f)) {
                    nn_model_free(model);
                    fclose(f);
                    return NULL;
                }

                if (use_bias) {
                    if (data->biases == NULL || !read_exact(data->biases->data.f32_data, sizeof(float), (size_t)units, f)) {
                        nn_model_free(model);
                        fclose(f);
                        return NULL;
                    }
                } else {
                    tensor_free(data->biases);
                    data->biases = NULL;
                }
            }
        }

        fclose(f);
        return model;
    }
}

/**
 * Free model and all resources
 * 
 * @param model         Neural network model
 */
void nn_model_free(NNModel* model) {
    if (model == NULL) return;
    
    /* Free all layers */
    for (int i = 0; i < model->layer_count; i++) {
        if (model->layers[i] != NULL) {
            model->layers[i]->free(model->layers[i]->layer_data);
            free(model->layers[i]);
        }
    }
    
    free(model->layers);
    free(model->name);
    free(model);
}

/* ═══════════════════════════════════════════════════════════════════
 * ONNX EXPORT/IMPORT (Model Serialization)
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Export model to ONNX format
 * 
 * This is a simplified ONNX export that creates a basic protobuf-like
 * structure. Full ONNX export would require a protobuf library.
 * 
 * @param model         Neural network model
 * @param filepath      Output file path
 * @return             True if successful
 */
bool nn_model_export_onnx(NNModel* model, const char* filepath) {
    if (model == NULL || filepath == NULL) return false;
    
    FILE* f = fopen(filepath, "wb");
    if (f == NULL) {
        fprintf(stderr, "[ONNX] Cannot open file for export: %s\n", filepath);
        return false;
    }
    
    /* Write ONNX header (magic number + version) */
    uint32_t magic = 0x0A0A0A0A;  /* Custom ONNX-like header */
    uint32_t version = 8;  /* ONNX version */
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);
    
    /* Write model metadata */
    uint32_t name_len = strlen(model->name);
    fwrite(&name_len, sizeof(uint32_t), 1, f);
    fwrite(model->name, 1, name_len, f);
    
    /* Write model properties */
    fwrite(&model->layer_count, sizeof(int), 1, f);
    
    /* Write IR version (ONNX IR v8) */
    int64_t ir_version = 8;
    fwrite(&ir_version, sizeof(int64_t), 1, f);
    
    /* Write producer info */
    const char* producer_name = "Vexium";
    const char* producer_version = "2.1.0";
    uint32_t pname_len = strlen(producer_name);
    uint32_t pver_len = strlen(producer_version);
    fwrite(&pname_len, sizeof(uint32_t), 1, f);
    fwrite(producer_name, 1, pname_len, f);
    fwrite(&pver_len, sizeof(uint32_t), 1, f);
    fwrite(producer_version, 1, pver_len, f);
    
    /* Write each layer */
    for (int i = 0; i < model->layer_count; i++) {
        Layer* layer = model->layers[i];
        if (layer == NULL) continue;
        
        /* Write layer type */
        int layer_type = layer->type;
        fwrite(&layer_type, sizeof(int), 1, f);
        
        /* Write activation type */
        int activation = layer->activation;
        fwrite(&activation, sizeof(int), 1, f);
        
        /* For dense layers, extract weights and biases */
        if (layer->type == LAYER_DENSE && layer->layer_data != NULL) {
            DenseLayerData* data = (DenseLayerData*)layer->layer_data;
            
            /* Write weight shape and data */
            int has_weights = (data->weights != NULL) ? 1 : 0;
            fwrite(&has_weights, sizeof(int), 1, f);
            
            if (has_weights && data->weights != NULL) {
                Tensor* w = data->weights;
                fwrite(&w->shape[0], sizeof(int), 1, f);
                fwrite(&w->shape[1], sizeof(int), 1, f);
                int total_elements = w->shape[0] * w->shape[1];
                fwrite(w->data.f32_data, sizeof(float), total_elements, f);
            }
            
            /* Write bias shape and data */
            int has_bias = (data->biases != NULL) ? 1 : 0;
            fwrite(&has_bias, sizeof(int), 1, f);
            
            if (has_bias && data->biases != NULL) {
                Tensor* b = data->biases;
                fwrite(&b->shape[0], sizeof(int), 1, f);
                fwrite(b->data.f32_data, sizeof(float), b->shape[0], f);
            }
        } else {
            /* No weights for other layer types */
            int has_weights = 0, has_bias = 0;
            fwrite(&has_weights, sizeof(int), 1, f);
            fwrite(&has_bias, sizeof(int), 1, f);
        }
    }
    
    /* Write footer marker */
    uint32_t footer = 0xFFFFFFFF;
    fwrite(&footer, sizeof(uint32_t), 1, f);
    
    fclose(f);
    printf("[ONNX] Model exported to: %s\n", filepath);
    return true;
}

/**
 * Import model from ONNX format
 * 
 * @param filepath      Input file path
 * @return             Loaded model or NULL
 */
NNModel* nn_model_import_onnx(const char* filepath) {
    if (filepath == NULL) return NULL;
    
    FILE* f = fopen(filepath, "rb");
    if (f == NULL) {
        fprintf(stderr, "[ONNX] Cannot open file for import: %s\n", filepath);
        return NULL;
    }
    
    /* Read and verify header */
    uint32_t magic, version;
    fread(&magic, sizeof(uint32_t), 1, f);
    fread(&version, sizeof(uint32_t), 1, f);
    
    if (magic != 0x0A0A0A0A) {
        fprintf(stderr, "[ONNX] Invalid file format\n");
        fclose(f);
        return NULL;
    }
    
    /* Read model name */
    uint32_t name_len;
    fread(&name_len, sizeof(uint32_t), 1, f);
    char* name = malloc(name_len + 1);
    fread(name, 1, name_len, f);
    name[name_len] = '\0';
    
    NNModel* model = nn_model_new(name);
    free(name);
    
    if (model == NULL) {
        fclose(f);
        return NULL;
    }
    
    /* Read layer count from file; model starts with layer_count = 0 */
    int file_layer_count = 0;
    fread(&file_layer_count, sizeof(int), 1, f);
    
    /* Skip IR version */
    int64_t ir_version;
    fread(&ir_version, sizeof(int64_t), 1, f);
    
    /* Skip producer info */
    uint32_t pname_len, pver_len;
    fread(&pname_len, sizeof(uint32_t), 1, f);
    fseek(f, pname_len, SEEK_CUR);
    fread(&pver_len, sizeof(uint32_t), 1, f);
    fseek(f, pver_len, SEEK_CUR);
    
    /* Read each layer */
    for (int i = 0; i < file_layer_count; i++) {
        int layer_type, activation;
        fread(&layer_type, sizeof(int), 1, f);
        fread(&activation, sizeof(int), 1, f);
        
        /* Read weights */
        int has_weights;
        fread(&has_weights, sizeof(int), 1, f);
        
        Tensor* weights = NULL;
        if (has_weights) {
            int w_rows, w_cols;
            fread(&w_rows, sizeof(int), 1, f);
            fread(&w_cols, sizeof(int), 1, f);
            
            int total_elements = w_rows * w_cols;
            float* weight_data = malloc(sizeof(float) * total_elements);
            fread(weight_data, sizeof(float), total_elements, f);
            
            int shape[2] = {w_rows, w_cols};
            weights = tensor_new(TENSOR_FLOAT32, 2, shape);
            memcpy(weights->data.f32_data, weight_data, sizeof(float) * total_elements);
            free(weight_data);
        }
        
        /* Read bias */
        int has_bias;
        fread(&has_bias, sizeof(int), 1, f);
        
        Tensor* biases = NULL;
        if (has_bias) {
            int b_dim;
            fread(&b_dim, sizeof(int), 1, f);
            
            float* bias_data = malloc(sizeof(float) * b_dim);
            fread(bias_data, sizeof(float), b_dim, f);
            
            int shape[1] = {b_dim};
            biases = tensor_new(TENSOR_FLOAT32, 1, shape);
            memcpy(biases->data.f32_data, bias_data, sizeof(float) * b_dim);
            free(bias_data);
        }
        
        /* Create layer with loaded weights */
        if (layer_type == LAYER_DENSE && weights != NULL) {
            int output_dim = weights->shape[1];
            nn_model_add_dense(model, output_dim, activation, weights->shape[0]);
            
            /* Copy loaded weights to the layer */
            if (model->layer_count > 0 && model->layers[model->layer_count - 1] != NULL && model->layers[model->layer_count - 1]->layer_data != NULL) {
                DenseLayerData* data = (DenseLayerData*)model->layers[model->layer_count - 1]->layer_data;
                if (data->weights != NULL) tensor_free(data->weights);
                data->weights = weights;
                if (data->biases != NULL) tensor_free(data->biases);
                data->biases = biases;
                weights = NULL;
                biases = NULL;
            }
        }

        tensor_free(weights);
        tensor_free(biases);
    }
    
    fclose(f);
    printf("[ONNX] Model imported from: %s\n", filepath);
    return model;
}

/* ═══════════════════════════════════════════════════════════════════
 * OPTIMIZERS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Create an optimizer
 * 
 * @param type          Optimizer type
 * @param lr            Learning rate
 * @return             Optimizer instance
 */
void* optimizer_create(OptimizerType type, double lr) {
    switch (type) {
        case OPTIMIZER_SGD: {
            SGDOptimizer* opt = (SGDOptimizer*)malloc(sizeof(SGDOptimizer));
            if (opt == NULL) return NULL;
            opt->lr = lr;
            opt->momentum = 0.9;
            opt->decay = 0.0;
            return opt;
        }
        
        case OPTIMIZER_ADAM: {
            AdamOptimizer* opt = (AdamOptimizer*)malloc(sizeof(AdamOptimizer));
            if (opt == NULL) return NULL;
            opt->lr = lr;
            opt->beta1 = 0.9;
            opt->beta2 = 0.999;
            opt->epsilon = 1e-8;
            opt->t = 0;
            opt->m = NULL;
            opt->v = NULL;
            return opt;
        }
        
        case OPTIMIZER_RMSPROP: {
            RMSpropOptimizer* opt = (RMSpropOptimizer*)malloc(sizeof(RMSpropOptimizer));
            if (opt == NULL) return NULL;
            opt->lr = lr;
            opt->rho = 0.9;
            opt->epsilon = 1e-8;
            opt->avg_sq_grad = NULL;
            return opt;
        }
        
        default:
            return NULL;
    }
}

/**
 * Update parameters using optimizer
 * 
 * @param opt           Optimizer instance
 * @param type          Optimizer type
 * @param param         Parameter tensor to update
 * @param grad          Gradient tensor
 */
void optimizer_update(void* opt, OptimizerType type, Tensor* param, Tensor* grad) {
    if (opt == NULL || param == NULL || grad == NULL) return;
    
    switch (type) {
        case OPTIMIZER_SGD: {
            SGDOptimizer* sgd = (SGDOptimizer*)opt;
            if (!tensor_same_shape(param, grad) || param->dtype != TENSOR_FLOAT32 || grad->dtype != TENSOR_FLOAT32) break;
            for (size_t i = 0; i < param->size; i++) {
                param->data.f32_data[i] -= (float)(sgd->lr * (double)grad->data.f32_data[i]);
            }
            break;
        }
        
        case OPTIMIZER_ADAM: {
            AdamOptimizer* adam = (AdamOptimizer*)opt;
            if (!tensor_same_shape(param, grad) || param->dtype != TENSOR_FLOAT32 || grad->dtype != TENSOR_FLOAT32) break;
            if (adam->m == NULL || !tensor_same_shape(adam->m, grad)) {
                tensor_free(adam->m);
                adam->m = tensor_zeros_like(grad);
            }
            if (adam->v == NULL || !tensor_same_shape(adam->v, grad)) {
                tensor_free(adam->v);
                adam->v = tensor_zeros_like(grad);
            }
            if (adam->m == NULL || adam->v == NULL) break;
            adam->t++;

            {
                double bias_correction1 = 1.0 - pow(adam->beta1, adam->t);
                double bias_correction2 = 1.0 - pow(adam->beta2, adam->t);
                if (fabs(bias_correction1) < 1e-12) bias_correction1 = 1.0;
                if (fabs(bias_correction2) < 1e-12) bias_correction2 = 1.0;

                for (size_t i = 0; i < param->size; i++) {
                    double g = (double)grad->data.f32_data[i];
                    double m = adam->beta1 * (double)adam->m->data.f32_data[i] + (1.0 - adam->beta1) * g;
                    double v = adam->beta2 * (double)adam->v->data.f32_data[i] + (1.0 - adam->beta2) * g * g;
                    double m_hat = m / bias_correction1;
                    double v_hat = v / bias_correction2;

                    adam->m->data.f32_data[i] = (float)m;
                    adam->v->data.f32_data[i] = (float)v;
                    param->data.f32_data[i] -= (float)(adam->lr * m_hat / (sqrt(v_hat) + adam->epsilon));
                }
            }
            break;
        }
        
        case OPTIMIZER_RMSPROP: {
            RMSpropOptimizer* rmsprop = (RMSpropOptimizer*)opt;
            if (!tensor_same_shape(param, grad) || param->dtype != TENSOR_FLOAT32 || grad->dtype != TENSOR_FLOAT32) break;
            if (rmsprop->avg_sq_grad == NULL || !tensor_same_shape(rmsprop->avg_sq_grad, grad)) {
                tensor_free(rmsprop->avg_sq_grad);
                rmsprop->avg_sq_grad = tensor_zeros_like(grad);
            }
            if (rmsprop->avg_sq_grad == NULL) break;

            for (size_t i = 0; i < param->size; i++) {
                double g = (double)grad->data.f32_data[i];
                double avg = rmsprop->rho * (double)rmsprop->avg_sq_grad->data.f32_data[i] + (1.0 - rmsprop->rho) * g * g;
                rmsprop->avg_sq_grad->data.f32_data[i] = (float)avg;
                param->data.f32_data[i] -= (float)(rmsprop->lr * g / (sqrt(avg) + rmsprop->epsilon));
            }
            break;
        }
    }
}

/**
 * Free optimizer
 * 
 * @param opt           Optimizer instance
 * @param type          Optimizer type
 */
void optimizer_free(void* opt, OptimizerType type) {
    if (opt == NULL) return;
    
    switch (type) {
        case OPTIMIZER_SGD:
            free(opt);
            break;
        case OPTIMIZER_ADAM: {
            AdamOptimizer* adam = (AdamOptimizer*)opt;
            tensor_free(adam->m);
            tensor_free(adam->v);
            free(adam);
            break;
        }
        case OPTIMIZER_RMSPROP: {
            RMSpropOptimizer* rmsprop = (RMSpropOptimizer*)opt;
            tensor_free(rmsprop->avg_sq_grad);
            free(rmsprop);
            break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * VEXIUM VM INTEGRATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Get tensor from VM value (helper for VM integration)
 * 
 * This function would be used by the VM to extract tensor objects
 * from Value representations.
 * 
 * @param value         VM Value
 * @return             Tensor pointer or NULL
 */
Tensor* value_to_tensor(Value value) {
    /* In a full implementation, this would check if the value
     * is a tensor object and extract the pointer */
    UNUSED(value);
    return NULL;
}

/**
 * Create VM value from tensor
 * 
 * This function would be used by the VM to wrap tensor objects
 * in Value representations for the stack.
 * 
 * @param tensor        Tensor pointer
 * @return             VM Value
 */
Value tensor_to_value(Tensor* tensor) {
    /* In a full implementation, this would wrap the tensor in
     * a Value that can be pushed to the VM stack */
    UNUSED(tensor);
    /* Return a nothing value for now */
    return val_nothing_v();
}

/*
 * Note: The tensor_register_natives function is commented out as it requires
 * full VM integration. Native function placeholders are kept for future use.
 * 
 * void tensor_register_natives(VM* vm) {
 *     if (vm == NULL) return;
 *     ...
 * }
 */

/* ═══════════════════════════════════════════════════════════════════
 * PLACEHOLDER NATIVE FUNCTIONS (for VM integration)
 * ═══════════════════════════════════════════════════════════════════ */

static Value native_tensor_new(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_zeros(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_ones(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_rand(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_randn(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_add(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_sub(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_mul(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_div(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_matmul(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_reshape(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_transpose(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_sum(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_mean(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_std(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_max(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_min(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_relu(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_sigmoid(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_tanh(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_tensor_softmax(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_new(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_add_dense(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_forward(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_predict(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_train(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}

static Value native_nn_model_free(int argCount, Value* args) {
    UNUSED(argCount);
    UNUSED(args);
    return val_nothing_v();
}
