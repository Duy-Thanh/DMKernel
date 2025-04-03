#ifndef DM_PRIMITIVES_H
#define DM_PRIMITIVES_H

#include "../dmkernel.h"

// Primitive function registration
dm_error_t dm_register_primitives(dm_context_t *ctx);

// Matrix operations
dm_error_t dm_prim_matrix_create(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_get(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_set(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_add(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_sub(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_mul(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_transpose(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_matrix_inverse(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Statistical functions
dm_error_t dm_prim_mean(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_median(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_std_dev(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_variance(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_correlation(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_covariance(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Machine learning primitives
dm_error_t dm_prim_kmeans(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_knn(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_linear_regression(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_decision_tree(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Signal processing
dm_error_t dm_prim_fft(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_ifft(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_wavelet(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_filter(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Data I/O operations
dm_error_t dm_prim_load_csv(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_save_csv(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_load_json(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_save_json(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

// Earthquake-specific primitives
dm_error_t dm_prim_eq_load_usgs(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_eq_detect_patterns(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_eq_predict_aftershocks(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);
dm_error_t dm_prim_eq_analyze_magnitude(dm_context_t *ctx, int argc, dm_value_t *argv, dm_value_t *result);

#endif /* DM_PRIMITIVES_H */ 