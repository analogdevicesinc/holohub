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

    [prob, result] = classifyModulation(data.rx);

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

%% Gen Code lib
cfg = coder.gpuConfig('lib');
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
for k  = 1:10
for mode = 1:length(modulationTypes)
    data = load(['mod_',char(modulationTypes(mode)),'.mat']);

    [prob, result] = classifyModulation_mex(data.rx);

    disp(prob);
    disp(result);
    disp(modulationTypes(mode));
    disp('------');

end
end
