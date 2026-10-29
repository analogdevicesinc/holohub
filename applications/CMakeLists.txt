# SPDX-FileCopyrightText: Copyright (c) 2026 Analog Devices, Inc. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Each application is gated on its own APP_<name> option, defaulting to ${BUILD_ALL}.
# Add one add_holohub_application() call per application; declare operator
# dependencies via DEPENDS OPERATORS so they auto-enable when the app is built.
add_holohub_application(iio
    DEPENDS OPERATORS iio_controller
)
