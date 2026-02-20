import jupiter_config as config

def jupiter_init(sdrs):
    sdrs.load_phase_cal()
    sdrs.load_gain_cal()
    if (config.used_rx_channels > 0) and (config.used_rx_channels <= 4):
        sdrs.num_rx_elements = config.used_rx_channels
    else:
        print("WARNING: Wrong number of used_rx_channels! Modify config file!")
        sdrs.num_rx_elements = 4

    sdrs.tx_destroy_buffer()
    sdrs.rx_destroy_buffer()

    sdrs.rx_enabled_channels = config.rx_channels_used
    sdrs.tx_enabled_channels = config.tx_channels_used

    sdrs.rx_ensm_mode_chan0 = "rf_enabled"
    sdrs.rx_ensm_mode_chan1 = "rf_enabled"

    # Config LOs
    sdrs.rx0_lo = config.lo_freq
    sdrs.rx1_lo = config.lo_freq
    sdrs.tx0_lo = config.lo_freq
    sdrs.tx1_lo = config.lo_freq

    if config.rx_gain_control_mode == "automatic":
        sdrs.gain_control_mode_chan0 = config.rx_gain_control_mode
        sdrs.gain_control_mode_chan1 = config.rx_gain_control_mode
    elif config.rx_gain_control_mode == "spi":
        sdrs.rx_hardwaregain_all_chan0 = config.rx_gain
        sdrs.rx_hardwaregain_all_chan1 = config.rx_gain

    sdrs.tx_hardwaregain_all_chan0 = config.tx1_gain
    sdrs.tx_hardwaregain_all_chan1 = config.tx2_gain
    sdrs.primary.atten_control_mode_chan0 = "spi"
    sdrs.primary.tx_hardwaregain_chan0 = 0

    sdrs.primary.tx_cyclic_buffer = True

    for dev in [sdrs.primary] + sdrs.secondaries:
        dev.rx_buffer_size = config.rx_buffer_size
        dev._tx_buffer_size = config.tx_buffer_size