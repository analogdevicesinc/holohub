function [v, modulation] = classifyModulation(inputSig) %#codegen
assert(isa(inputSig, 'int16') && all(size(inputSig) == [8192, 1]));

coder.gpu.kernelfun();
% input signal size is 8192-by-1

% parameters
ModelFile = 'mod_classify_model.mat'; % file that saves the neural network model
% imSize = [1024 1]; % Size of the input image for the deep learning network

%Function to converts signal to wavelet time-frequency image
% im = cwtModType(inputSig, imSize);

%Load the trained deep learning network
persistent model;
if isempty(model)
    model = coder.loadDeepLearningNetwork(ModelFile, 'mynet');
end

% Changes CJ
i_samples = single(inputSig(1:2:end-1));
q_samples = single(inputSig(2:2:end));

selectedChannel = complex(i_samples, q_samples);

framePower = mean(abs(selectedChannel).^2);
selectedChannel = selectedChannel / sqrt(framePower);
%End Changes

inputSigI = real(selectedChannel);
inputSigQ = imag(selectedChannel);
inputSigIQ = [inputSigI, inputSigQ];

% Predict the Signal Modulation
ldIn = dlarray(inputSigIQ,"TC");
dlA = model.predict(ldIn);
predClassProb = extractdata(dlA);
% predClassProb = model.predict(inputSig);

[v,i] = max(predClassProb);
% disp(predClassProb)
% disp(i)
%mods = ["16QAM", "64QAM", "8PSK", "BPSK", "CPFSK", "GFSK", "PAM4", "QPSK"];
% mods = ["16QAM", "64QAM", "8PSK", "B-FM", "BPSK", "CPFSK", "DSB-AM", "GFSK", "PAM4", "QPSK", "SSB-AM"];
% modulation = mods(i);
modulation=i;


end