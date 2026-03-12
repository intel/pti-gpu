import torch

# check if XPU is present
if not torch.xpu.is_available():
    print("[ERROR] XPU device is not present.")
else:
    # generate two random 4x4 matrices with single precision on XPU
    xpu_a = torch.rand((4, 4), dtype=torch.float32, device="xpu")
    xpu_b = torch.rand((4, 4), dtype=torch.float32, device="xpu")

    # matrix multiplication on XPU
    xpu_c = torch.matmul(xpu_a, xpu_b)

    # copy the data back to CPU for validation
    cpu_a = xpu_a.to("cpu")
    cpu_b = xpu_b.to("cpu")
    cpu_c = xpu_c.to("cpu")

    # validation
    if torch.allclose(cpu_c, torch.matmul(cpu_a, cpu_b)):
        print("[INFO] Matrix multiplication is correct!")
    else:
        print("[ERROR] Matrix multiplication validation failed.")