# SPDX-FileCopyrightText: Copyright (c) 2025 Analog Devices, Inc. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import math
import os
from typing import List, Sequence

import numpy as np
import matplotlib.pyplot as plt
from collections import deque

from adrv9002_multi_wrapper import adrv9002_multi
import jupiter_config as config
from jupiter_init import jupiter_init
from jupiter_utils import (
    generate_tx_sinewave,
    adjust_gain,
    adjust_phase,
    measure_phase_degrees,
    calibrate_boresight
)

import iio
from datetime import timedelta

from holoscan.conditions import CountCondition, PeriodicCondition
from holoscan.core import Application, Operator, OperatorSpec
from holoscan.operators import HolovizOp

from holohub.iio_controller import (
    IIOAttributeRead,
    IIOAttributeWrite,
    IIOBufferInfo,
    IIOBufferRead,
    IIOBufferWrite,
    IIOChannelInfo,
    IIOConfigurator,
    IIODataFormat,
)

G_JUPITER_PRIMARY_URI = f"ip:{config.jupiter_ip_primary}"
G_JUPITER_SECONDARY_URI = f"ip:{config.jupiter_ip_secondary}"
G_SYNCHRONA_URI = f"ip:{config.synchrona_ip}"

class SynchronizedRxCaptureOperator(Operator):
    """Operator that performs full synchronized RX using sdrs.rx() method."""

    def __init__(self, fragment, *args, sdrs=None, **kwargs):
        super().__init__(fragment, *args, **kwargs)
        self.sdrs = sdrs

    def setup(self, spec: OperatorSpec):
        """Setup the operator."""
        spec.output("rx_channels")

    def compute(self, op_input, op_output, context):
        """Compute method using native synchronized RX."""
        if self.sdrs is None:
            print("ERROR: SDR reference not provided")
            return

        # This single call handles: rx_arm(), buffer prep, sysref_request, and capture!
        iq_data = self.sdrs.rx()  # Returns list of 4 complex arrays [ch0, ch1, ch2, ch3]

        # Apply calibrations in correct order
        iq_data = adjust_gain(self.sdrs, iq_data)
        iq_data = adjust_phase(self.sdrs, 10, iq_data)  # 90 degree phase shift for clear visibility

        # Debug: print phase differences to verify they're being applied
        for ch in range(1, len(iq_data)):
            phase_diff = measure_phase_degrees(iq_data[0], iq_data[ch])

        # Emit the calibrated complex channel data
        op_output.emit(iq_data, "rx_channels")

class WaveformToHolovizOp(Operator):
    """Converts complex IQ channel data to line strip geometry for HolovizOp visualization."""

    # Channel names and colors for legend
    CHANNEL_NAMES = ["Ch0 (Primary RX1)", "Ch1 (Primary RX2)", "Ch2 (Secondary RX1)", "Ch3 (Secondary RX2)"]
    CHANNEL_COLORS = ["RED", "GREEN", "BLUE", "YELLOW"]

    def __init__(self, fragment, *args, downsample=1, **kwargs):
        super().__init__(fragment, *args, **kwargs)
        self.downsample = downsample  # 1 = no downsampling, full resolution
        self.frame_count = 0
        self.num_samples = 0  # Store for axis labels

    def setup(self, spec: OperatorSpec):
        spec.input("rx_channels")
        spec.output("holoviz_output")

    def compute(self, op_input, op_output, context):
        """Convert channel data to HolovizOp line strip format."""
        channel_data = op_input.receive("rx_channels")

        if channel_data is None or len(channel_data) == 0:
            print("WaveformToHolovizOp: No data received!")
            return

        self.frame_count += 1

        # Convert to list if needed (Holoscan might wrap it)
        if hasattr(channel_data, '__iter__') and not isinstance(channel_data, (list, np.ndarray)):
            channel_data = list(channel_data)

        num_channels = len(channel_data)
        total_samples = len(channel_data[0])

        # Limit to first 512 samples for detailed view
        max_display_samples = 512
        num_samples = min(total_samples, max_display_samples)

        self.num_samples = num_samples

        # No downsampling - show all samples up to limit
        sample_indices = np.arange(0, num_samples)
        num_points = num_samples

        # X coordinates normalized to [0, 1]
        x_coords = sample_indices / num_samples

        # Find global min/max across all channels for consistent scaling (first N samples only)
        all_i_samples = [np.real(channel_data[ch])[:num_samples] for ch in range(min(num_channels, 4))]
        global_min = min(np.min(s) for s in all_i_samples)
        global_max = max(np.max(s) for s in all_i_samples)
        global_range = global_max - global_min if global_max != global_min else 1.0

        # Build output dictionary for HolovizOp
        output = {}

        for ch_idx in range(min(num_channels, 4)):  # Max 4 channels
            i_samples = all_i_samples[ch_idx]

            # Normalize Y to [0.1, 0.9] - all channels overlapping on same scale
            y_normalized = (i_samples - global_min) / global_range * 0.8 + 0.1

            # Create line strip vertices as (x, y) pairs - shape (N, 2)
            vertices = np.column_stack([x_coords, y_normalized]).astype(np.float32)
            output[f"waveform_ch{ch_idx}"] = vertices

        # Add legend label positions (top-right corner)
        output["label_ch0"] = np.array([[0.75, 0.95]], dtype=np.float32)
        output["label_ch1"] = np.array([[0.75, 0.91]], dtype=np.float32)
        output["label_ch2"] = np.array([[0.75, 0.87]], dtype=np.float32)
        output["label_ch3"] = np.array([[0.75, 0.83]], dtype=np.float32)

        # Show first 3 normalized Y values being sent to HolovizOp
        for ch in range(min(num_channels, 4)):
            y_vals = output[f"waveform_ch{ch}"][:3, 1]  # First 3 Y coordinates

        op_output.emit(output, "holoviz_output")


class MyApp(Application):
    def __init__(self, *args, **kwargs):
        """Init the application."""
        super().__init__(*args, *kwargs)
        self.sdrs = adrv9002_multi(
            primary_uri=config.jupiter_ips[0],
            secondary_uris=config.jupiter_ips[1:],
            sync_uri=config.synchrona_ip,
            # enable_ssh=True,
            # sshargs={"username": "root", "password": "analog"},
            # profile_path=os.path.join(os.path.dirname(__file__),"MCS_30_72_CLK_AND_RATE.json")
        )

        jupiter_init(self.sdrs)
        calibrate_boresight(self.sdrs)
        # Start cyclic TX transmission (critical - must happen before RX!)
        import time
        _, tx_samples = generate_tx_sinewave()
        self.sdrs.primary.tx(tx_samples)
        time.sleep(0.5)  # Wait for TX to stabilize
        self.name = "Jupiter Sine Sync Example"

    def compose(self):
        """Compose the application."""
        FPS = 60
        # TX is already started in __init__, RX uses native sdrs.rx() method
        TIMEDELAY = 1000 // FPS  # Convert FPS to ms delay
        # Create synchronized RX capture operator with periodic condition for slower updates
        periodic_cond = PeriodicCondition(self, recess_period=timedelta(milliseconds=TIMEDELAY))
        sync_rx_op = SynchronizedRxCaptureOperator(
            self,
            periodic_cond,
            sdrs=self.sdrs,
            name="synchronized_rx_capture"
        )

        # Create waveform formatter for HolovizOp
        waveform_formatter = WaveformToHolovizOp(self, name="waveform_formatter")

        # Create HolovizOp for visualization
        # Channel colors: Ch0=Red, Ch1=Green, Ch2=Blue, Ch3=Yellow
        viz = HolovizOp(
            self,
            name="visualizer",
            width=1280,
            height=720,
            window_title="Jupiter Sine Sync Example",
            tensors=[
                # Waveform lines - thicker for better visibility
                dict(name="waveform_ch0", type="line_strip", opacity=1.0, line_width=3, color=[1.0, 0.2, 0.2, 1.0]),
                dict(name="waveform_ch1", type="line_strip", opacity=1.0, line_width=3, color=[0.2, 1.0, 0.2, 1.0]),
                dict(name="waveform_ch2", type="line_strip", opacity=1.0, line_width=3, color=[0.2, 0.2, 1.0, 1.0]),
                dict(name="waveform_ch3", type="line_strip", opacity=1.0, line_width=3, color=[1.0, 1.0, 0.2, 1.0]),
                # Legend labels
                dict(name="label_ch0", type="text", opacity=1.0, color=[1.0, 0.2, 0.2, 1.0], text=["Ch0 (Primary RX1)"]),
                dict(name="label_ch1", type="text", opacity=1.0, color=[0.2, 1.0, 0.2, 1.0], text=["Ch1 (Primary RX2)"]),
                dict(name="label_ch2", type="text", opacity=1.0, color=[0.2, 0.2, 1.0, 1.0], text=["Ch2 (Secondary RX1)"]),
                dict(name="label_ch3", type="text", opacity=1.0, color=[1.0, 1.0, 0.2, 1.0], text=["Ch3 (Secondary RX2)"]),
            ],
        )

        # Flow: RX capture → Waveform formatter → HolovizOp
        self.add_flow(sync_rx_op, waveform_formatter, {("rx_channels", "rx_channels")})
        self.add_flow(waveform_formatter, viz, {("holoviz_output", "receivers")})

if __name__ == "__main__":
    config_file = os.path.join(os.path.dirname(__file__), "iio_config.yaml")
    app = MyApp()
    app.config(config_file)
    app.run()
