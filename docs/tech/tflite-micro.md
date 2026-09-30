# TensorFlow Lite Micro

TFLite Micro is a C++ inference runtime for microcontrollers. A FlatBuffer model is compiled into firmware, a fixed tensor arena is supplied, `AllocateTensors()` plans activation memory, and `Invoke()` runs inference without dynamic allocation.

HELIOS uses an operator resolver containing only the model's convolution, depthwise convolution, arithmetic, pooling, reshape, fully connected, softmax, and activation operators. The exact resolver must match the converted model.

| Region | Size |
|---|---:|
| INT8 model in flash | 42 KB |
| Tensor arena allocated/used | 96 KB / 51 KB |
| Ring buffer | 64 KB |
| MFCC scratch | 8 KB |

The fixed arena gives deterministic memory behavior. An undersized or incorrectly aligned arena makes allocation fail; a missing resolver operator causes runtime failure. See `firmware/arduino/HELIOS/kws_engine.cpp`.
