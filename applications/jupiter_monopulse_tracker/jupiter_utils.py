import numpy as np
import time
import jupiter_config as config

def measure_phase_degrees(chan0, chan1):
    errorV = np.angle(chan0 * np.conj(chan1)) * 180 / np.pi
    error = np.mean(errorV)
    return error

def generate_tx_sinewave():
    # Calculate time values
    t = np.arange(config.num_samps) / config.sample_rate
   
    # Generate sinusoidal waveform
    phase_shift = -np.pi/2  # Shift by -90 degrees
    samples = config.amplitude_discrete * (np.cos(2 * np.pi * config.tx_sine_baseband_freq * t + phase_shift) + 1j*np.sin(2 * np.pi * config.tx_sine_baseband_freq * t + phase_shift))
    # window = np.hanning(len(samples))
    # samples *= window
    return t, samples

def adjust_gain(jupiter_obj, samples_list):
    if len(samples_list) != config.used_rx_channels:
        print("WARNING: Wrong number of input arrays, check used_rx_channels in the config file!")
        return 0
    return [samples * jupiter_obj.gcal[i] for i, samples in enumerate(samples_list)]

def adjust_phase(jupiter_obj, phase_shift_deg, samples_list):
    if len(samples_list) != config.used_rx_channels:
        print("WARNING: Wrong number of input arrays, check used_rx_channels in the config file!")
        return 0
    # First channel is the reference channel and is not shifted
    adjusted_samples = [samples_list[0]]
    for i in range(1, len(samples_list)):
        phase_rad = np.deg2rad(((phase_shift_deg * i) + jupiter_obj.pcal[i-1]) % 360.0)
        adjusted_samples.append(samples_list[i] * np.exp(1j * phase_rad))
    
    return adjusted_samples

def do_cal_gain(sdrs):
    #################################################
    # Create and plot a complex sinusoid ############
    #################################################
    t, tx_samples = generate_tx_sinewave()

    ##################################################
    # Call Tx function to start transmission #########
    ##################################################
    sdrs.primary.tx(tx_samples)

    time.sleep(1) # wait for internal calibrations
    # Clear buffer just to be safe
    for _ in range (0, 2):
        raw_data = sdrs.rx()

    #############################################################
    # Call Rx function to receive transmission and plot the data#
    #############################################################
    rx_samples = sdrs.rx()
    time.sleep(1) # wait for internal calibrations

    # Save received amplitudes from each channel
    amplitudes = np.array([np.max(np.abs(rx_samples[i])) for i in range(sdrs.num_rx_elements)])
    # amplitudes = np.array([np.max(np.real(rx_samples[i])) for i in range(sdrs.num_rx_elements)])
    elem_with_max_amplitude = np.argmax(amplitudes)
    max_amplitude = amplitudes[elem_with_max_amplitude]

    print("Amplitudes list: " + str(amplitudes))

    # Calculate the calibration coefficents between the amplitude on the channel
    # with max amplitude and other channels
    amplitude_cal_coeff = max_amplitude / amplitudes
    amplitude_cal_coeff[elem_with_max_amplitude] = 1.0

    # Save gain calibration coefficents and print them
    sdrs.gcal = amplitude_cal_coeff.tolist()
    print("Gain calibration coefficents: " + str(amplitude_cal_coeff))
    sdrs.save_gain_cal()

    # Stop transmitting
    sdrs.tx_destroy_buffer()

def do_cal_phase(sdrs):
    # Create and plot a complex sinusoid #######################################
    ############################################################################
    ############################################################################
    t, tx_samples = generate_tx_sinewave()

    ################################################################
    # Call Tx function to start transmission #######################
    ################################################################
    sdrs.primary.tx(tx_samples)

    time.sleep(1) # wait for internal calibrations
    # Clear buffer just to be safe
    for _ in range (0, 2):
        raw_data = sdrs.rx()

    ################################################################
    # Call Rx function to receive transmission and plot the data####
    ################################################################
    # Receive and plot time domain data before calibration
    rx_samples = sdrs.rx()

    # Calculate phase differences for all channels relative to ch0
    repeat_ph_calculations = 10
    num_channels = sdrs.num_rx_elements - 1  # Exclude reference channel 0
    phase_diffs = [[] for _ in range(num_channels)]

    for iteration in range(repeat_ph_calculations):
        rx_samples = sdrs.rx()
        print(f"Iteration {iteration}:")
        for ch in range(1, sdrs.num_rx_elements):
            ph_diff = measure_phase_degrees(rx_samples[0], rx_samples[ch])
            phase_diffs[ch-1].append(ph_diff)
            print(f"Ph Diff Between ch0 and ch{ch}: {ph_diff}")

    # Calculate statistics using numpy
    avg_phase_diffs = [np.mean(diffs) for diffs in phase_diffs]
    max_phase_diffs = [np.max(diffs) for diffs in phase_diffs]
    min_phase_diffs = [np.min(diffs) for diffs in phase_diffs]

    # Save calibration
    sdrs.pcal = avg_phase_diffs
    print(f"pcal values: {sdrs.pcal}")
    sdrs.save_phase_cal()

    # Print statistics
    for ch in range(num_channels):
        print(f"Avg ph diff for ch0 - ch{ch+1}: {avg_phase_diffs[ch]}")
    for ch in range(num_channels):
        print(f"Max diff in phase ch0-ch{ch+1}: {max_phase_diffs[ch]}")
        print(f"Min diff in phase ch0-ch{ch+1}: {min_phase_diffs[ch]}")
 
    # Stop transmitting
    sdrs.tx_destroy_buffer()

def calibrate_boresight(sdrs):
    print("If you are using antennas, place the trasmitting antenna at boresight")
    print("Starting application level phase calibration...")
    do_cal_phase(sdrs)
    print("Starting application level gain calibration...")
    do_cal_gain(sdrs)
