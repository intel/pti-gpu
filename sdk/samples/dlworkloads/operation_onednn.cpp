//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <oneapi/dnnl/dnnl.hpp>
#include <oneapi/dnnl/dnnl_sycl.hpp>

#include "operation_onednn.h"
#include "utils.h"

// code as simple as possible for the demo
namespace {
inline auto& Conv2dWeightsInstance() {
    static TinyTensor conv2d_weights(0, 0, 0, 0);
    return conv2d_weights;
}
} // namespace 

void onednn_prepare_weights(int oc, int ic, int ks, sycl::queue *q)
{
    TinyTensor tt(oc, ic, ks, ks);
    Conv2dWeightsInstance() = tt;

    float *weights_h = new float[Conv2dWeightsInstance().count()];
    for (size_t i = 0; i < Conv2dWeightsInstance().count(); ++i) {
        weights_h[i] = random_float();
    }

    q->memcpy(Conv2dWeightsInstance().data, weights_h, Conv2dWeightsInstance().count() * sizeof(float)).wait();
    delete [] weights_h;
}

dnnl::engine& get_engine(sycl::queue *q)
{
    // static for dev and ctx?
    sycl::device dev = q->get_device();
    sycl::context ctx(dev);
    // is it safe when q is released at the end of the process?
    static dnnl::engine eng = dnnl::sycl_interop::make_engine(dev, ctx);
    return eng;
}

dnnl::stream& get_stream(dnnl::engine& eng, sycl::queue *q)
{
    // is it safe when q is released at the end of the process?
    static dnnl::stream s = dnnl::sycl_interop::make_stream(eng, *q);
    return s;
}

TinyTensor run_onednn_operation_conv2d(const TinyTensor& inp, sycl::queue *q)
{
    dnnl::engine eng;
    dnnl::stream s;
    try {
        eng = get_engine(q);
        s = get_stream(eng, q);
    } catch(const dnnl::error& e) {
        std::cerr << "Unable to get dnnl resources, failed with status: " << e.status << '\n';
        throw;
    }

    TinyTensor outp(inp.N, Conv2dWeightsInstance().N, inp.H, inp.W);

    // for the simple case, don't use reorder
    auto src_md = dnnl::memory::desc(
            {inp.N, inp.C, inp.H, inp.W},
            dnnl::memory::data_type::f32,
            dnnl::memory::format_tag::nchw
    );

    auto weights_md = dnnl::memory::desc(
            {outp.C, inp.C, Conv2dWeightsInstance().H, Conv2dWeightsInstance().W},
            dnnl::memory::data_type::f32,
            dnnl::memory::format_tag::nchw
    );

    auto bias_md = dnnl::memory::desc();

    auto dst_md = dnnl::memory::desc(
            {outp.N, outp.C, outp.H, outp.W},
            dnnl::memory::data_type::f32,
            dnnl::memory::format_tag::nchw
    );

    dnnl::primitive_attr pattr;
    auto conv_pd = dnnl::convolution_forward::primitive_desc(
                eng,
                dnnl::prop_kind::forward_inference,
                dnnl::algorithm::convolution_auto,
                src_md,
                weights_md,
                bias_md,
                dst_md,
                {1, 1}, // strides
                {0, 0}, // dilations
                {1, 1}, {1, 1}, // left and right padding
                pattr);

    auto src_mem = dnnl::sycl_interop::make_memory(
            src_md,
            eng,
            dnnl::sycl_interop::memory_kind::usm,
            inp.data);

    auto weights_mem = dnnl::sycl_interop::make_memory(
            weights_md,
            eng,
            dnnl::sycl_interop::memory_kind::usm,
            Conv2dWeightsInstance().data);

    auto dst_mem = dnnl::sycl_interop::make_memory(
            dst_md,
            eng,
            dnnl::sycl_interop::memory_kind::usm,
            outp.data);

    assert(conv_pd.src_desc() == src_mem.get_desc());
    assert(conv_pd.dst_desc() == dst_mem.get_desc());
    assert(conv_pd.weights_desc() == weights_mem.get_desc());

    int scratchpad_size = conv_pd.scratchpad_desc().get_size();
    static bool warning_shown = false;
    if (scratchpad_size == 0) {
        if (!warning_shown) {
            warning_shown = true;
            // std::cout << __FILE__ << ":" << __LINE__;
            // std::cout << " we need a onednn case that scratchpad_size > 0, to verify if it can be allocated within onednn for sycl grapch capture mode" << std::endl;
        }
    }

    auto conv = dnnl::convolution_forward(conv_pd);
    conv.execute(s,
                {{DNNL_ARG_SRC, src_mem},
                 {DNNL_ARG_WEIGHTS, weights_mem},
                 {DNNL_ARG_DST, dst_mem}});

    return outp;
}
