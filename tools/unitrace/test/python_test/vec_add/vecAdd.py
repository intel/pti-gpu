import torch

# check if XPU is present
if not torch.xpu.is_available():
  print("[ERROR] XPU device is not present.")

# generate random tensor data with single precision data on xpu device
xpu_a = torch.rand(32, dtype=torch.float32, device="xpu")
xpu_b = torch.rand(32, dtype=torch.float32, device="xpu")

# execute element wise add on XPU
for i in range(10):
  xpu_c = xpu_a + xpu_b

# copy the result and other data back to CPU for validation
cpu_a = xpu_a.to("cpu")
cpu_b = xpu_b.to("cpu")
cpu_c = xpu_c.to("cpu")

# validation
if torch.allclose(cpu_c, cpu_a + cpu_b):
  print("[INFO] vector addition is correct!")
else:
  print("[ERROR] validation failed!")
