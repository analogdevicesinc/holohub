%% Generate model for classification function
matfile = 'mod_classify_model.mat';
% load trainedModulationClassificationNetwork
load mod_classify_model_split_iq.mat
save(matfile,'trainedNet');

%% Test MATLAB
modulationTypes = categorical(sort(["BPSK", "QPSK", "8PSK", ...
  "16QAM", "64QAM", "PAM4", "GFSK", "CPFSK"]));


for mode = 1:length(modulationTypes)
    data = load(['talise_c1qam_data.mat']);

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

%% Gen Code mex
cfg = coder.gpuConfig('mex');
cfg.GpuConfig.CompilerFlags = '--fmad=false';
cfg.GenerateReport = true;
% cfg.DeepLearningConfig = coder.DeepLearningConfig('cudnn');
cfg.DeepLearningConfig = coder.DeepLearningConfig('none');
% cfg.DeepLearningConfig.AutoTuning = true;
codegen -config cfg classifyModulation

%% Gen Code dll
cfg = coder.gpuConfig('dll','ecoder',true);
cfg.GpuConfig.CompilerFlags = '--fmad=false';
cfg.GenerateReport = true;
% cfg.DeepLearningConfig = coder.DeepLearningConfig('cudnn');
cfg.DeepLearningConfig = coder.DeepLearningConfig('none');
% cfg.DeepLearningConfig.AutoTuning = true;
codegen -config cfg classifyModulation


%% Test
for k  = 1
for mode = 1:length(modulationTypes)
    data = load(['talise_c1qam_data.mat']);

    data1.rx = reshape([data.axi_adrv9009_rx_hpc_voltage0_i';data.axi_adrv9009_rx_hpc_voltage0_q';...
                        data.axi_adrv9009_rx_hpc_voltage1_i';data.axi_adrv9009_rx_hpc_voltage1_q';...
                        data.axi_adrv9009_rx_hpc_voltage2_i';data.axi_adrv9009_rx_hpc_voltage2_q';...
                        data.axi_adrv9009_rx_hpc_voltage3_i';data.axi_adrv9009_rx_hpc_voltage3_q'],...
                        [],1);
    data1.rx = int16(data1.rx);
    [prob, result] = classifyModulation_mex(data1.rx, uint8(1));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation_mex(data1.rx, uint8(2));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation_mex(data1.rx, uint8(3));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

    [prob, result] = classifyModulation_mex(data1.rx, uint8(4));
    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

end
end

% disp('REAL DATA');
% data = load(['osc_capt.mat']);
% data1 = complex(single(data.axi_adrv9009_rx_hpc_voltage0_i), single(data.axi_adrv9009_rx_hpc_voltage0_q));
% 
% [prob, result] = classifyModulation_mex(data1);
% 
% disp(prob);
% disp(result);
% disp('------');
