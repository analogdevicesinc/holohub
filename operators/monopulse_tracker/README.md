# Monopulse Tracker Operator

## Overview

The Monopulse Tracker operator implements monopulse tracking for phased array antenna systems. It calculates sum and delta beams from phase-adjusted IQ data, computes phase error via correlation, and updates the tracking phase to estimate angle of arrival.

## Description

Monopulse tracking is a radar technique that uses the amplitude and/or phase comparison of signals received by multiple antenna elements to determine the angular position of a target. This operator implements the amplitude-comparison monopulse technique for a 4-element linear phased array.

### Algorithm

1. **Sum Beam**: Combines all 4 channels coherently (ch0 + ch1 + ch2 + ch3)
2. **Delta Beam**: Difference between left and right pairs ((ch0 + ch1) - (ch2 + ch3))
3. **Correlation**: Cross-correlates sum and delta beams to extract phase error
4. **Phase Update**: Adjusts tracking phase based on error sign
5. **Steering Angle**: Converts phase to physical angle using array geometry

## Requirements

### Software
- Holoscan SDK 2.6+
- CUDA Toolkit

### Hardware
- NVIDIA GPU (Jetson Xavier/Orin or discrete GPU)
- 4-channel phased array receiver (e.g., Jupiter SDR system)

## Implementation

The operator uses CUDA for GPU-accelerated signal processing:
- Sum/delta beam calculation runs in parallel on the GPU
- Correlation uses thrust parallel reduction
- Only final phase update and steering angle calculation run on CPU

## Inputs

| Name | Type | Description |
|------|------|-------------|
| `phase_adjusted_channels` | `holoscan::gxf::Entity` containing `holoscan::Tensor` | Complex IQ data with shape [4, num_samples], dtype complex64 |

## Outputs

| Name | Type | Description |
|------|------|-------------|
| `steer_angle` | `holoscan::gxf::Entity` containing `holoscan::Tensor` | Estimated steering angle in degrees (1-element float tensor) |
| `current_phase` | `holoscan::gxf::Entity` containing `holoscan::Tensor` | Updated tracking phase in degrees (1-element float tensor) for feedback loop |

## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `element_spacing` | float | (required) | Antenna element spacing in meters |
| `signal_frequency` | float | (required) | Signal frequency in Hz |
| `phase_step` | float | 5.0 | Phase adjustment step size in degrees |
| `current_phase` | float | 0.0 | Initial phase in degrees |
| `number_of_channels` | uint8 | 4 | Number of receive channels |
| `number_of_samples_per_channel` | uint32 | 16384 | Samples per channel |
| `sample_size` | ssize_t | 8 | Size of each sample in bytes (complex64 = 8) |

## Usage Example

### Python

```python
from holohub.monopulse_tracker import MonopulseTracker

# Create operator
tracker = MonopulseTracker(
    self,
    element_spacing=0.028,  # meters
    signal_frequency=5.4e9,  # Hz
    phase_step=5.0,
    name="monopulse_tracker"
)

# Connect in pipeline
self.add_flow(phase_adjuster, tracker, {("phase_adjusted_channels", "phase_adjusted_channels")})
self.add_flow(tracker, visualizer, {("steer_angle", "steer_angle")})
```

### C++

```cpp
auto tracker = make_operator<ops::MonopulseTracker>(
    "monopulse_tracker",
    Arg("element_spacing", 0.028f),
    Arg("signal_frequency", 5.4e9f),
    Arg("phase_step", 5.0f)
);

add_flow(phase_adjuster, tracker, {{"phase_adjusted_channels", "phase_adjusted_channels"}});
add_flow(tracker, visualizer, {{"steer_angle", "steer_angle"}});
```

## See Also

- [Jupiter Monopulse Tracker Application](../../applications/jupiter_monopulse_tracker) - Complete application example
