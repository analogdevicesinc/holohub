% Specify IP, username and p/w of device
clear hwobj
hwobj = jetson('10.48.65.174','analog','analog');

%% Generate model for classification function
matfile = 'mod_classify_model.mat';
% load trainedModulationClassificationNetwork
load mod_classify_model_split_iq.mat
save(matfile,'trainedNet');

%% Test MATLAB
modulationTypes = categorical(sort(["BPSK", "QPSK", "8PSK", ...
  "16QAM", "64QAM", "PAM4", "GFSK", "CPFSK"]));


for mode = 1:length(modulationTypes)
    data = load(['mod_',char(modulationTypes(mode)),'.mat']);
    data1.rx = single(data.rx)


    [prob, result] = classifyModulation(data1.rx);

    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

end

%% Gen Code dll
cfg = coder.gpuConfig('dll','ecoder',true);
cfg.GpuConfig.CompilerFlags = '--fmad=false';
cfg.GenerateReport = true;
% cfg.DeepLearningConfig = coder.DeepLearningConfig('cudnn');
cfg.DeepLearningConfig = coder.DeepLearningConfig('none');
% cfg.DeepLearningConfig.AutoTuning = true;
cfg.Hardware = coder.hardware('NVIDIA Jetson');
codegen -config cfg classifyModulation