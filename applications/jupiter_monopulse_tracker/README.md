# Jupiter Monopulse Tracker

## Overview

This application demonstrates real-time monopulse tracking using a 4-element phased array antenna system with Jupiter SDR hardware. It captures synchronized IQ data from multiple receivers, applies calibration, and performs angle-of-arrival estimation using the monopulse technique.

## Description

Monopulse tracking is a radar technique that determines the angular position of a target by comparing signals received by multiple antenna elements. This application implements amplitude-comparison monopulse for a 4-element linear phased array.

### Pipeline Architecture

```
SynchronizedRxCaptureOperator -> PhaseOperator -> MonopulseTracker (C++) -> WaveformToHolovizOp -> HolovizOp
```

1. **SynchronizedRxCaptureOperator**: Captures IQ data from all 4 RX channels with MCS synchronization
2. **PhaseOperator**: Applies gain/phase calibration and steering phase
3. **MonopulseTracker**: Computes sum/delta beams and estimates angle of arrival (C++ implementation)
4. **WaveformToHolovizOp**: Formats tracking data for visualization
5. **HolovizOp**: Real-time display of tracking results

## Hardware Requirements

- **2x Jupiter SDR boards** (ADRV9002-based)
  - Primary board handles TX and 2 RX channels
  - Secondary board provides 2 additional RX channels
- **1x Synchrona board** (HMC7044-based) for multi-chip synchronization
- **4-element phased array antenna** for receive
- **Directional antenna** for transmit (or power splitter for testing)

## Software Requirements

- Holoscan SDK 2.6+
- libiio
- pyadi-iio
- NumPy

## Configuration

Edit `jupiter_config.py` to match your hardware setup:

```python
# Board IP addresses
synchrona_ip = "10.48.65.203"
jupiter_ip_primary = "10.48.65.188"
jupiter_ip_secondary = "10.48.65.187"

# RF parameters
lo_freq = 5400000000 - 300000  # LO frequency in Hz
tx_sine_baseband_freq = 300000  # Baseband test signal frequency
lambda_over_d_spacing = 1.93    # Antenna element spacing ratio
```

## Calibration

Before running the tracker, calibrate the system at boresight:

```python
from jupiter_utils import calibrate_boresight
calibrate_boresight(sdrs)
```

This saves calibration files:
- `phase_cal_val.pkl` - Phase differences between channels
- `gain_cal_val.pkl` - Gain correction coefficients

## Running the Application

```bash
# Build the monopulse_tracker operator first
./holohub build monopulse_tracker

# Run the application
python3 tracker.py
```

## Visualization

The application displays a real-time plot showing:
- X-axis: Steering angle (-180° to +180°)
- Y-axis: Time (newest at bottom)
- Blue line: Tracking history

## Files

| File | Description |
|------|-------------|
| `tracker.py` | Main Holoscan application |
| `jupiter_config.py` | Hardware configuration parameters |
| `jupiter_init.py` | SDR initialization routines |
| `jupiter_utils.py` | Calibration and signal processing utilities |
| `adrv9002_multi_wrapper.py` | Multi-chip ADRV9002 wrapper class |

## See Also

- [Monopulse Tracker Operator](../../operators/monopulse_tracker) - C++ operator documentation
- [PyADI-IIO](https://github.com/analogdevicesinc/pyadi-iio) - Python bindings for ADI devices
