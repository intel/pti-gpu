//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <string.h>

#include "pti/pti_view.h"
#include "samples_c_utils.h"

const unsigned long long kRequestedRecordCount = 64ULL;
const unsigned long long kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

void StartTracing() {
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DRIVER_API));
  PTI_CHECK_SUCCESS(ptiViewEnableDriverApiClass(1, PTI_API_CLASS_GPU_OPERATION_CORE,
                                                PTI_API_GROUP_LEVELZERO));
}

void StopTracing() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DRIVER_API));
}

void ProvideBuffer(unsigned char **buf, size_t *buf_size) {
  *buf = (unsigned char*)malloc(kRequestedBufferSize);
  if (!*buf) {
    fprintf(stderr, "Unable to allocate buffer for PTI tracing\n");
    exit(EXIT_FAILURE);
  }
  *buf_size = kRequestedBufferSize;
}

void ParseBuffer(unsigned char *buf, size_t buf_size, size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    fprintf(stderr, "Received empty buffer\n");
    if (valid_buf_size) {
      free(buf);
    }
    return;
  }
  pti_view_record_base *ptr = NULL;
  while (true) {
    pti_result buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == PTI_STATUS_END_OF_BUFFER) {
      printf("Reached End of buffer\n");
      break;
    }
    if (buf_status != PTI_SUCCESS) {
      fprintf(stderr, "Found Error Parsing Records from PTI\n");
      break;
    }
    if (ptr->_view_kind == PTI_VIEW_INVALID) {
      printf("Found Invalid Record\n");
      continue;
    }
    switch(ptr->_view_kind) {
      case PTI_VIEW_DRIVER_API: {
        printf("--------------------------------------------------------------------------------\n");
        printf("Found Driver Api Record\n");
        DumpRecordApi((pti_view_record_api *)ptr);
        printf("--------------------------------------------------------------------------------\n");
        break;
      }
      case PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        printf("--------------------------------------------------------------------------------\n");
        printf("Found Memory Record\n");
        DumpRecordMemoryCopy((pti_view_record_memory_copy*)ptr);
        printf("--------------------------------------------------------------------------------\n");
        break;
      }
      case PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        printf("--------------------------------------------------------------------------------\n");
        printf("Found Memory Record\n");
        DumpRecordMemoryFill((pti_view_record_memory_fill*)ptr);
        printf("--------------------------------------------------------------------------------\n");
        break;
      }
      case PTI_VIEW_DEVICE_GPU_KERNEL: {
        bool with_sycl_info = false;
        pti_view_record_kernel *rec = (pti_view_record_kernel*)ptr;
        printf("--------------------------------------------------------------------------------\n");
        printf("Found Kernel Record\n");
        DumpRecordKernel(rec, with_sycl_info);
        printf("--------------------------------------------------------------------------------\n");
        uint64_t exec_time [] = {rec->_append_timestamp,
                                 rec->_submit_timestamp, rec->_start_timestamp,
                                 rec->_end_timestamp};
        if (IsMonotonicUint64(exec_time, 4)) {
          printf("------------>     All Monotonic\n");
        } else {
          printf("------------>     Something wrong: NOT All monotonic\n");
        }
        break;
      }
      default: {
        fprintf(stderr, "This shouldn't happen\n");
        break;
      }
    }
  }
  free(buf);
}

const unsigned max_size = 8192;
const unsigned min_size = 32;

void Usage(const char *name) {
  printf(" Calculating floating point matrix multiply on gpu\n");
  printf("%s [ [gpu|cpu|host, default=gpu],  [matrix size, default=1024, max=%u], [repetition count, default=4]] \n", 
         name, max_size);
}

void Compute(unsigned int size) {
  const unsigned int N = (size > 0) ? size : 2048;
  
  // Allocate arrays using standard C malloc
  int* a = (int*)malloc(N * sizeof(int));
  int* b = (int*)malloc(N * sizeof(int));
  int* c = (int*)malloc(N * sizeof(int));
  
  if (!a || !b || !c) {
    fprintf(stderr, "Memory allocation failed\n");
    free(a);
    free(b);
    free(c);
    exit(EXIT_FAILURE);
  }

  // Initialize arrays
  for(unsigned int i = 0; i < N; ++i) {
    a[i] = i;
    b[i] = i * 2;
  }

  // Initialize result array to zero
  memset(c, 0, N * sizeof(int));

  // Use direct pointers for OpenMP target mapping
  int* A = a;
  int* B = b;
  int* C = c;

  // GPU code using OpenMP target directives
  #pragma omp target map(to: A[0:N], B[0:N]) map(from: C[0:N])
  #pragma omp parallel for
  for (unsigned int i = 0; i < N; ++i) {
    C[i] = A[i] + B[i];
  }

  // Verify results
  for (unsigned int i = 0; i < N; ++i) {
    if (((unsigned int)C[i]) != (3 * i)) {
      fprintf(stderr, "Data mismatch found. Expected %d but actual %d\n", 3 * i, C[i]);
      free(a);
      free(b);
      free(c);
      exit(EXIT_FAILURE);
    }
  }

  // Clean up memory
  free(a);
  free(b);
  free(c);
}

int main(int argc, char *argv[]) {

  // Prolog - PTI SDK setup
  // 1. Set callbacks for buffer management
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
  // 2. Start Tracing
  StartTracing();

  // Default values
  unsigned int repeat_count = 1;
  unsigned int size = 2048;

  unsigned long temp = size;
  if (argc > 1) {
    char* endptr;
    temp = strtoul(argv[1], &endptr, 10);
    if (*endptr != '\0' || endptr == argv[1]) {
      fprintf(stderr, "Invalid number format for size: %s\n", argv[1]);
      temp = size; // Use default value on error
    }
    size = (temp < min_size) ? min_size : (temp > max_size) ? max_size : temp;
  }
  if (argc > 2) {
    char* endptr;
    temp = strtoul(argv[2], &endptr, 10);
    if (*endptr != '\0' || endptr == argv[2]) {
      fprintf(stderr, "Invalid number format for repeat count: %s\n", argv[2]);
      temp = 1; // Use default value on error
    }
    repeat_count = (temp < 1) ? 1 : temp;
  }

  for (unsigned i = 0; i < repeat_count; ++i) {
    // let's use omp method to compute
    Compute(size);
  }

  //Epilog - PTI SDK teardown
  StopTracing();
  PTI_CHECK_SUCCESS(ptiFlushAllViews());

  printf("Profiling is stopped\n");
  printf("Application is completed successfully\n");
  return 0;
}
