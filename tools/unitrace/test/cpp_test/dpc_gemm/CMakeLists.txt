project(dpc_gemm CXX)

add_executable(dpc_gemm main.cc)
add_sycl_to_target(dpc_gemm)


# Line tables for both host and device code, so stall samples carry a source location.
#
# The two -g here are not the same flag and only one of them may go away:
#
#   -g on icpx itself is what puts debug info into the device image at all. icpx ignores
#   -gline-tables-only for spir64 ("only supported for host compilation"), so without -g
#   there is no line information in the SPIR-V and stall samples get no source location.
#
#   -g inside -Xs is what IGC reads, and it is the one that hurts. Full -g on the backend
#   disables inlining and forces calls out of line; a callee is not a kernel, so it gets no
#   .kprops record and its samples cannot be attributed to anything. With it, this kernel
#   profiled as ~5 KB of argument marshalling charged to the lambda's declaration line and
#   99% of the sampled stall counts were discarded - the arithmetic never appeared at all.
#   -gline-tables-only asks IGC for the line table without that codegen change.
#   -O2 is not optional. This project sets no CMAKE_BUILD_TYPE, so without it there is no
#   optimization level at all, and icpx warns that a debug flag with no -O behaves like -O0
#   - which is a second, independent reason for nothing to be inlined. README.md's recipe
#   (icpx -fsycl -gline-tables-only -O2) has it for this reason.
target_compile_options(dpc_gemm PRIVATE -O2 -g -gline-tables-only)
target_link_options(dpc_gemm PRIVATE -O2 -g -Xs "-gline-tables-only")

install(TARGETS dpc_gemm RUNTIME DESTINATION bin/dpc_gemm COMPONENT dpc_gemm)
