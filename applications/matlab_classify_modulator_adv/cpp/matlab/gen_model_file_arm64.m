% Specify IP, username and p/w of device
clear hwobj
hwobj = jetson('10.48.65.198','analog','analog');

%% Generate model for classification function
matfile = 'mod_classify_model.mat';
% load trainedModulationClassificationNetwork
load mod_classify_model_split_iq.mat
save(matfile,'trainedNet');

%% Test MATLAB
modulationTypes = categorical(sort(["BPSK", "QPSK", "8PSK", ...
  "16QAM", "64QAM", "PAM4", "GFSK", "CPFSK"]));


for mode = 1:length(modulationTypes)
    % data = load(['mod_',char(modulationTypes(mode)),'.mat']);
    data = load(['talise_c1qam_data.mat']);

    % data.rx = repelem(data.rx, 4);
    % data1.rx = single(data.rx);
    data1.rx = reshape([data.axi_adrv9009_rx_hpc_voltage0_i';data.axi_adrv9009_rx_hpc_voltage0_q';...
                        data.axi_adrv9009_rx_hpc_voltage1_i';data.axi_adrv9009_rx_hpc_voltage1_q';...
                        data.axi_adrv9009_rx_hpc_voltage2_i';data.axi_adrv9009_rx_hpc_voltage2_q';...
                        data.axi_adrv9009_rx_hpc_voltage3_i';data.axi_adrv9009_rx_hpc_voltage3_q'],...
                        [],1);
    data1.rx = int16(data1.rx);
    [prob, result] = classifyModulation(data1.rx, uint8(1));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation(data1.rx, uint8(2));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation(data1.rx, uint8(3));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation(data1.rx, uint8(4));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

end

%% Gen Code dll
cfg = coder.gpuConfig('dll','ecoder',true);
cfg.GpuConfig.CompilerFlags = '--fmad=false';
cfg.GpuConfig.MallocMode = 'discrete';
cfg.GenerateReport = true;
% cfg.DeepLearningConfig = coder.DeepLearningConfig('cudnn');
cfg.DeepLearningConfig = coder.DeepLearningConfig('none');
% cfg.DeepLearningConfig.AutoTuning = true;
cfg.Hardware = coder.hardware('NVIDIA Jetson');
codegen -config cfg classifyModulation
