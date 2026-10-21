# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
import sys
import time

try:
    import torch
    from torch import nn
except ModuleNotFoundError as e:
    if e.name != "torch":
        raise
    print(f"PyTorch is not installed for {sys.executable}")
    sys.exit(77)

if not torch.xpu.is_available():
    print("PyTorch XPU device is not available")
    sys.exit(77)

device = "xpu"
torch.manual_seed(0)

model = nn.Sequential(
    nn.Conv2d(3, 32, 3, padding=1),
    nn.ReLU(),
    nn.MaxPool2d(2),
    nn.Conv2d(32, 64, 3, padding=1),
    nn.ReLU(),
    nn.MaxPool2d(2),
    nn.Flatten(),
    nn.Linear(64 * 16 * 16, 256),
    nn.ReLU(),
    nn.Linear(256, 10),
).to(device)
optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
loss_fn = nn.CrossEntropyLoss()

inputs = torch.randn(128, 3, 64, 64, device=device)
targets = torch.randint(0, 10, (128,), device=device)

start = time.perf_counter()
for _ in range(50):
    optimizer.zero_grad()
    loss = loss_fn(model(inputs), targets)
    loss.backward()
    optimizer.step()
torch.xpu.synchronize()
print(
    f"PyTorch XPU training: loss {loss.item():.4f}, {time.perf_counter() - start:.2f} s"
)
