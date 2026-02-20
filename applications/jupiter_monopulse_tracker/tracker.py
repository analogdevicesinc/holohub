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

"""
Jupiter Monopulse Tracker Application

This application demonstrates real-time monopulse tracking using a 4-element
phased array antenna system with Jupiter SDR hardware. The application:

1. Captures synchronized IQ data from 4 RX channels
2. Applies gain and phase calibration
3. Performs monopulse tracking to estimate angle of arrival
4. Visualizes the tracking results in real-time

Hardware Requirements:
- 2x Jupiter SDR boards (primary + secondary)
- 1x Synchrona board for MCS synchronization
- 4-element phased array antenna
"""

import time
import os
import numpy as np
from datetime import timedelta

from holoscan.conditions import PeriodicCondition
from holoscan.core import Application, Operator, OperatorSpec
from holoscan.operators import HolovizOp
from holohub.monopulse_tracker import MonopulseTracker

from adrv9002_multi_wrapper import adrv9002_multi
import jupiter_config as config
from jupiter_init import jupiter_init
from jupiter_utils import (
    generate_tx_sinewave,
    adjust_gain,
    adjust_phase,
)

# Global shared phase state for feedback loop between operators
G_CURRENT_PHASE = 0.0


class SynchronizedRxCaptureOperator(Operator):
    """Operator that captures synchronized RX data from all 4 channels.

    Uses the sdrs.rx() method which handles: rx_arm(), buffer prep,
    sysref_request, and synchronized capture across both Jupiter boards.
    """

    def __init__(self, fragment, *args, sdrs=None, **kwargs):
        super().__init__(fragment, *args, **kwargs)
        self.sdrs = sdrs

    def setup(self, spec: OperatorSpec):
        spec.output("rx_channels")

    def compute(self, op_input, op_output, context):
        if self.sdrs is None:
            return

        # Synchronized capture from all 4 channels
        iq_data = self.sdrs.rx()

        # Apply calibrations
        iq_data = adjust_gain(self.sdrs, iq_data)

        op_output.emit(iq_data, "rx_channels")


class PhaseOperator(Operator):
    """Operator that applies phase calibration and steering phase to channels.

    On initialization, performs a phase sweep to find the initial boresight
    direction. During operation, applies the current tracking phase from
    the global G_CURRENT_PHASE variable.
    """

    def __init__(
        self,
        fragment,
        *args,
        sdrs=None,
        signal_freq=0,
        elem_spacing=0,
        **kwargs,
    ):
        global G_CURRENT_PHASE
        super().__init__(fragment, *args, **kwargs)
        self.sdrs = sdrs
        self.signal_freq = signal_freq
        self.elem_spacing = elem_spacing

        # Initial phase sweep to find boresight
        self._find_initial_phase()

    def _find_initial_phase(self):
        """Perform phase sweep to find initial boresight direction."""
        global G_CURRENT_PHASE

        phase_range = np.arange(
            -360 / config.lambda_over_d_spacing,
            360 / config.lambda_over_d_spacing,
            2,
        )

        receive_samples = self.sdrs.rx()
        receive_samples = adjust_gain(self.sdrs, receive_samples)

        powers = []
        phase_angles = []

        for phase in phase_range:
            rx_samples = adjust_phase(self.sdrs, phase, receive_samples)
            phase_angles.append(phase)

            data_sum = sum(rx_samples)
            power_dB = 10 * np.log10(np.sum(np.abs(data_sum) ** 2))
            powers.append(power_dB)

        powers = np.array(powers)
        G_CURRENT_PHASE = phase_angles[np.argmax(powers)]

    def setup(self, spec: OperatorSpec):
        spec.input("rx_channels")
        spec.output("phase_adjusted_channels")

    def compute(self, op_input, op_output, context):
        global G_CURRENT_PHASE

        data = op_input.receive("rx_channels")
        if data is None:
            return

        # Apply gain and phase calibration
        data = adjust_gain(self.sdrs, data)
        data = adjust_phase(self.sdrs, G_CURRENT_PHASE, data)

        # Stack into contiguous array for C++ operator
        stacked_data = np.ascontiguousarray(
            np.stack(data, axis=0).astype(np.complex64)
        )

        # Emit as dictionary with empty key for C++ compatibility
        op_output.emit({"": stacked_data}, "phase_adjusted_channels")


class WaveformToHolovizOp(Operator):
    """Visualization operator that formats tracking data for Holoviz display.

    Maintains a rolling history of steering angles and formats them as
    a line strip for real-time visualization. Also updates G_CURRENT_PHASE
    for the feedback loop.
    """

    def __init__(
        self,
        fragment,
        *args,
        tracking_length=1000,
        **kwargs,
    ):
        super().__init__(fragment, *args, **kwargs)
        self.tracking_length = tracking_length
        self.tracking_angles = np.ones(self.tracking_length) * 180
        self.tracking_angles[:-1] = -180

    def setup(self, spec: OperatorSpec):
        spec.input("steer_angle")
        spec.input("current_phase")
        spec.output("holoviz_output")

    def _extract_float(self, msg, default=0.0):
        """Extract float value from Entity/dict/tensor format."""
        if isinstance(msg, dict):
            tensor = msg.get("", list(msg.values())[0])
            return float(np.asarray(tensor)[0])
        elif hasattr(msg, '__iter__') and not isinstance(msg, (int, float)):
            return float(np.asarray(msg)[0])
        return float(msg) if msg is not None else default

    def compute(self, op_input, op_output, context):
        global G_CURRENT_PHASE

        # Receive both outputs from C++ operator
        steer_angle = self._extract_float(op_input.receive("steer_angle"))
        current_phase = self._extract_float(op_input.receive("current_phase"), G_CURRENT_PHASE)

        # Update global phase for feedback loop
        G_CURRENT_PHASE = current_phase

        # Update tracking history
        self.tracking_angles = np.append(self.tracking_angles, steer_angle)
        self.tracking_angles = self.tracking_angles[1:]

        # Build line strip for Holoviz (y in [1,0], x normalized to [0.1,0.9])
        num_points = len(self.tracking_angles)
        y_coords = np.linspace(1.0, 0.0, num_points, dtype=np.float32)
        x_normalized = ((self.tracking_angles + 180.0) / 360.0) * 0.8 + 0.1
        vertices = np.column_stack([x_normalized, y_coords]).astype(np.float32)

        op_output.emit({"tracking_line": vertices}, "holoviz_output")


class JupiterMonopulseTrackerApp(Application):
    """Main application for Jupiter Monopulse Tracker."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.name = "Jupiter Monopulse Tracker"

        # Initialize SDR hardware
        self.sdrs = adrv9002_multi(
            primary_uri=config.jupiter_ips[0],
            secondary_uris=config.jupiter_ips[1:],
            sync_uri=config.synchrona_ip,
            profile_path=os.path.join(os.path.dirname(__file__),"MCS_30_72_CLK_AND_RATE.json")
        )

        jupiter_init(self.sdrs)

        # Calculate element spacing from frequency
        self.elem_spacing = (
            (3e8 / (config.lo_freq + config.tx_sine_baseband_freq))
            / config.lambda_over_d_spacing
        )
        self.signal_freq = config.lo_freq

        # Start TX transmission
        _, tx_samples = generate_tx_sinewave()
        self.sdrs.primary.tx(tx_samples)
        time.sleep(0.5)

    def compose(self):
        # Periodic RX capture at 10 Hz
        periodic_cond = PeriodicCondition(
            self, recess_period=timedelta(milliseconds=100)
        )

        sync_rx_op = SynchronizedRxCaptureOperator(
            self,
            periodic_cond,
            sdrs=self.sdrs,
            name="synchronized_rx_capture",
        )

        phase_adjuster = PhaseOperator(
            self,
            sdrs=self.sdrs,
            signal_freq=self.signal_freq,
            elem_spacing=self.elem_spacing,
            name="phase_adjuster",
        )

        # C++ operator for visualization output
        monopulse_tracker = MonopulseTracker(
            self,
            element_spacing=self.elem_spacing,
            signal_frequency=self.signal_freq,
            phase_step=5.0,
            name="monopulse_tracker",
        )

        waveform_formatter = WaveformToHolovizOp(
            self,
            tracking_length=1000,
            name="waveform_formatter",
        )

        viz = HolovizOp(
            self,
            name="visualizer",
            width=1280,
            height=720,
            window_title="Jupiter Monopulse Tracker",
            tensors=[
                dict(
                    name="tracking_line",
                    type="line_strip",
                    opacity=1.0,
                    line_width=3,
                    color=[0.2, 0.2, 1.0, 1.0],
                )
            ],
        )

        # Pipeline: RX capture -> Phase adjustment -> Monopulse tracking -> Visualization
        self.add_flow(sync_rx_op, phase_adjuster, {("rx_channels", "rx_channels")})
        self.add_flow(phase_adjuster, monopulse_tracker, {("phase_adjusted_channels", "phase_adjusted_channels")})
        self.add_flow(monopulse_tracker, waveform_formatter, {("steer_angle", "steer_angle"), ("current_phase", "current_phase")})
        self.add_flow(waveform_formatter, viz, {("holoviz_output", "receivers")})


if __name__ == "__main__":
    app = JupiterMonopulseTrackerApp()
    app.run()
