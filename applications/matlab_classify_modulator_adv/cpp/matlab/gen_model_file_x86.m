%% Generate model for classification function
matfile = 'mod_classify_model.mat';
% load trainedModulationClassificationNetwork
load mod_classify_model_split_iq.mat
save(matfile,'trainedNet');

%% Test MATLAB
modulationTypes = categorical(sort(["BPSK", "QPSK", "8PSK", ...   
    "16QAM", "64QAM", "PAM4", "GFSK", "CPFSK"]));

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
for k  = 1:1
for mode = 1:length(modulationTypes)
    data = load(['mod_',char(modulationTypes(mode)),'_ffsom_8k.mat']);

% Talise/FFSOM processing
    % i = int16(data.axi_adrv9009_rx_hpc_voltage0_i);
    % q = int16(data.axi_adrv9009_rx_hpc_voltage0_q);
    i = int16(data.axi_ad9084_rx_hpc_voltage0_i);
    q = int16(data.axi_ad9084_rx_hpc_voltage0_q);
    data.rx = zeros(8192, 1, 'int16');     % Preallocate the result
    data.rx(1:2:end) = i(:);          % Fill odd indices with A
    data.rx(2:2:end) = q(:);
% End processing

    [prob, result] = classifyModulation(data.rx);

    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

end
end
