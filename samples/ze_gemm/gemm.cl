__kernel void GEMM(__global float* a, __global float* b,
                    __global float* c, int size) {
  int j = get_global_id(0);
  int i = get_global_id(1);
  float sum = 0.0f;
  for (int k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}