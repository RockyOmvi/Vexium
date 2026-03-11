#ifndef VEXIUM_TENSOR_H
#define VEXIUM_TENSOR_H

#include "common.h"
#include "gc.h"
#include "vm.h"

/* Forward declaration for activation type */
typedef enum {
    ACTIVATION_NONE,
    ACTIVATION_RELU,
    ACTIVATION_SIGMOID,
    ACTIVATION_TANH,
    ACTIVATION_SOFTMAX
} ActivationType;

/* Layer types for neural networks */
typedef enum {
    LAYER_DENSE,
    LAYER_CONV2D,
    LAYER_DROPOUT,
    LAYER_MAXPOOL2D,
    LAYER_FLATTEN
} LayerType;

/* ══════════════════════════════════════════════════════════════
 *  TENSOR OBJECT (for AI/ML)
 * ══════════════════════════════════════════════════════════════ */

/* Tensor data types */
typedef enum {
    TENSOR_FLOAT32,   /* 32-bit float */
    TENSOR_FLOAT64,   /* 64-bit float (double) */
    TENSOR_INT32,     /* 32-bit integer */
    TENSOR_INT64,     /* 64-bit integer */
    TENSOR_UINT8,     /* 8-bit unsigned integer */
    TENSOR_BOOL       /* Boolean */
} TensorDType;

/* Tensor object structure */
typedef struct Tensor {
    Obj obj;
    
    TensorDType dtype;         /* Data type */
    int ndim;                  /* Number of dimensions */
    int* shape;                /* Shape of each dimension */
    int* strides;              /* Strides for each dimension */
    size_t size;               /* Total number of elements */
    
    union {
        float*  f32_data;
        double* f64_data;
        int32_t* i32_data;
        int64_t* i64_data;
        uint8_t* u8_data;
        bool*   b_data;
    } data;
    
    bool owns_data;            /* Whether tensor owns its data */
} Tensor;

/* ══════════════════════════════════════════════════════════════
 *  TENSOR FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

/* Create tensor with given shape */
Tensor* tensor_new(TensorDType dtype, int ndim, const int* shape);

/* Create tensor from data (borrows data) */
Tensor* tensor_from_data(TensorDType dtype, int ndim, const int* shape, void* data);

/* Create zero-initialized tensor */
Tensor* tensor_zeros(TensorDType dtype, int ndim, const int* shape);

/* Create one-initialized tensor */
Tensor* tensor_ones(TensorDType dtype, int ndim, const int* shape);

/* Create tensor filled with a constant value */
Tensor* tensor_full(TensorDType dtype, double value, int ndim, const int* shape);

/* Create identity matrix (2D tensor) */
Tensor* tensor_identity(int size, TensorDType dtype);

/* Create random tensor */
Tensor* tensor_rand(int ndim, const int* shape);

/* Create normally distributed random tensor */
Tensor* tensor_randn(int ndim, const int* shape, double mean, double std);

/* Clone tensor */
Tensor* tensor_clone(Tensor* t);

/* Reshape tensor (returns new tensor, original unchanged) */
Tensor* tensor_reshape(Tensor* t, int new_ndim, const int* new_shape);

/* Transpose tensor */
Tensor* tensor_transpose(Tensor* t);

/* Get element at indices (as double) */
double tensor_get_scalar(Tensor* t, const int* indices);

/* Set element at indices (from double) */
void tensor_set_scalar(Tensor* t, const int* indices, double value);

/* Set scalar value from double */
void tensor_set_scalar(Tensor* t, const int* indices, double value);

/* ══════════════════════════════════════════════════════════════
 *  TENSOR OPERATIONS (In-place)
 * ══════════════════════════════════════════════════════════════ */

/* Element-wise addition: t1 += t2 */
void tensor_iadd(Tensor* t1, Tensor* t2);

/* Element-wise subtraction: t1 -= t2 */
void tensor_isub(Tensor* t1, Tensor* t2);

/* Element-wise multiplication: t1 *= t2 */
void tensor_imul(Tensor* t1, Tensor* t2);

/* Element-wise division: t1 /= t2 */
void tensor_idiv(Tensor* t1, Tensor* t2);

/* Scalar addition: t += scalar */
void tensor_iadd_scalar(Tensor* t, double scalar);

/* Scalar multiplication: t *= scalar */
void tensor_imul_scalar(Tensor* t, double scalar);

/* Matrix multiplication: result = t1 @ t2 */
Tensor* tensor_matmul(Tensor* t1, Tensor* t2);

/* Sum all elements */
double tensor_sum(Tensor* t);

/* Mean of all elements */
double tensor_mean(Tensor* t);

/* Standard deviation */
double tensor_std(Tensor* t);

/* Maximum value */
double tensor_max(Tensor* t);

/* Minimum value */
double tensor_min(Tensor* t);

/* Apply activation function */
Tensor* tensor_relu(Tensor* t);
Tensor* tensor_sigmoid(Tensor* t);
Tensor* tensor_tanh(Tensor* t);
Tensor* tensor_softmax(Tensor* t);

/* Gradient computation */
Tensor* tensor_gradient(Tensor* loss, Tensor* target);

/* Auto-differentiation functions */
Tensor* tensor_grad_mse(Tensor* pred, Tensor* target);
Tensor* tensor_grad_cross_entropy(Tensor* pred, Tensor* target);
Tensor* tensor_grad_relu(Tensor* x, Tensor* grad_output);
Tensor* tensor_grad_sigmoid(Tensor* x, Tensor* grad_output);
Tensor* tensor_grad_tanh(Tensor* x, Tensor* grad_output);
void tensor_grad_matmul(Tensor* a, Tensor* b, Tensor* grad_output, 
                        Tensor** grad_a, Tensor** grad_b);

/* Free tensor */
void tensor_free(Tensor* t);

/* ══════════════════════════════════════════════════════════════
 *  NEURAL NETWORK LAYERS
 * ══════════════════════════════════════════════════════════════ */

typedef struct Layer Layer;

/* Dense (fully connected) layer */
typedef struct {
    Tensor* weights;    /* (input_size, output_size) */
    Tensor* biases;    /* (output_size,) */
    Tensor* input;     /* Cached input for backprop */
    bool use_bias;
} DenseLayer;

/* Convolutional layer */
typedef struct {
    int in_channels;
    int out_channels;
    int kernel_size;
    int stride;
    int padding;
    Tensor* kernels;   /* (out_channels, in_channels, kernel_size, kernel_size) */
    Tensor* biases;    /* (out_channels,) */
} Conv2DLayer;

/* Dropout layer */
typedef struct {
    double rate;       /* Dropout probability */
    Tensor* mask;      /* Binary mask */
} DropoutLayer;

/* Pooling layer */
typedef struct {
    int pool_size;
    int stride;
} MaxPool2DLayer;

/* ══════════════════════════════════════════════════════════════
 *  NEURAL NETWORK MODEL
 * ══════════════════════════════════════════════════════════════ */

typedef struct NNModel {
    char* name;
    Layer** layers;
    int layer_count;
    int layer_capacity;
    
    Tensor* input;
    Tensor* output;
    Tensor* loss;
    
    double learning_rate;
    bool training;
} NNModel;

typedef enum {
    LOSS_MSE,
    LOSS_CROSS_ENTROPY
} LossType;

/* Create new neural network model */
NNModel* nn_model_new(const char* name);

/* Add dense layer */
void nn_model_add_dense(NNModel* model, int units, ActivationType activation, int input_size_override);

/* Add convolutional layer */
void nn_model_add_conv2d(NNModel* model, int filters, int kernel_size, 
                         int stride, int padding, ActivationType activation);

/* Add dropout layer */
void nn_model_add_dropout(NNModel* model, double rate);

/* Add max pooling */
void nn_model_add_maxpool2d(NNModel* model, int pool_size, int stride);

/* Forward pass */
Tensor* nn_model_forward(NNModel* model, Tensor* input);

/* Backward pass (compute gradients) */
void nn_model_backward(NNModel* model, Tensor* loss);

/* Update weights using gradients */
void nn_model_update(NNModel* model);

/* Train one epoch */
double nn_model_train(NNModel* model, Tensor* X, Tensor* y, int batch_size);

/* Train one epoch with explicit loss type */
double nn_model_train_with_loss(NNModel* model, Tensor* X, Tensor* y, int batch_size, LossType loss_type);

/* Predict */
Tensor* nn_model_predict(NNModel* model, Tensor* input);

/* Learning rate controls */
void nn_model_set_learning_rate(NNModel* model, double learning_rate);
double nn_model_get_learning_rate(NNModel* model);

/* Save model to file (JSON format) */
bool nn_model_save(NNModel* model, const char* filepath);

/* Load model from file */
NNModel* nn_model_load(const char* filepath);

/* Export model to ONNX format (stub) */
bool nn_model_export_onnx(NNModel* model, const char* filepath);

/* Import model from ONNX format (stub) */
NNModel* nn_model_import_onnx(const char* filepath);

/* Free model */
void nn_model_free(NNModel* model);

/* ══════════════════════════════════════════════════════════════
 *  OPTIMIZERS
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    OPTIMIZER_SGD,
    OPTIMIZER_ADAM,
    OPTIMIZER_RMSPROP
} OptimizerType;

/* SGD optimizer */
typedef struct {
    double lr;
    double momentum;
    double decay;
} SGDOptimizer;

/* Adam optimizer */
typedef struct {
    double lr;
    double beta1;
    double beta2;
    double epsilon;
    int t;  /* Iteration count */
    Tensor* m;  /* First moment estimate */
    Tensor* v;  /* Second moment estimate */
} AdamOptimizer;

/* RMSprop optimizer */
typedef struct {
    double lr;
    double rho;
    double epsilon;
    Tensor* avg_sq_grad;
} RMSpropOptimizer;

/* Create optimizer */
void* optimizer_create(OptimizerType type, double lr);

/* Update parameters */
void optimizer_update(void* opt, OptimizerType type, Tensor* param, Tensor* grad);

/* Free optimizer */
void optimizer_free(void* opt, OptimizerType type);

#endif /* VEXIUM_TENSOR_H */
