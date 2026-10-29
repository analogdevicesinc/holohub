# SPDX-FileCopyrightText: Copyright (c) 2026, Analog Devices, Inc. All rights reserved.
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

from holoscan.core import Application
from holoscan.iio_controller import (
    IIOAttributeRead,
    IIOAttributeWrite,
    IIOBufferRead,
    IIOBufferWrite,
    IIOConfigurator,
)


class _TestApplication(Application):
    def compose(self):
        pass


def test_imports():
    for cls, name in [
        (IIOAttributeRead, "IIOAttributeRead"),
        (IIOAttributeWrite, "IIOAttributeWrite"),
        (IIOBufferRead, "IIOBufferRead"),
        (IIOBufferWrite, "IIOBufferWrite"),
        (IIOConfigurator, "IIOConfigurator"),
    ]:
        assert cls.__name__ == name


def test_construct_attribute_read():
    app = _TestApplication()
    op = IIOAttributeRead(app, ctx="ip:192.168.2.1", attr_name="ensm_mode")
    assert op is not None


def test_construct_attribute_write():
    app = _TestApplication()
    op = IIOAttributeWrite(app, ctx="ip:192.168.2.1", attr_name="ensm_mode")
    assert op is not None


def test_construct_buffer_read():
    app = _TestApplication()
    op = IIOBufferRead(
        app,
        ctx="ip:192.168.2.1",
        dev="cf-ad9361-lpc",
        is_cyclic=False,
        samples_count=8192,
        enabled_channel_names=["voltage0", "voltage1"],
        enabled_channel_input=[False, False],
    )
    assert op is not None


def test_construct_buffer_write():
    app = _TestApplication()
    op = IIOBufferWrite(
        app,
        ctx="ip:192.168.2.1",
        dev="cf-ad9361-dds-core-lpc",
        is_cyclic=True,
        enabled_channel_names=["voltage0", "voltage1"],
        enabled_channel_output=[True, True],
    )
    assert op is not None


def test_construct_configurator():
    app = _TestApplication()
    op = IIOConfigurator(app, cfg="iio_config.yaml")
    assert op is not None
