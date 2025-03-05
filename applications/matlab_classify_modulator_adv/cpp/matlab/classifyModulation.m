function [v, modulation] = classifyModulation(inputSig, chan) %#codegen
assert(isa(inputSig, 'int16') && all(size(inputSig) == [8192, 1]));

assert(isa(chan, 'uint8') && isreal(chan) && all(size(chan) == [1,1]), ...
    'Channel number must be an integer between 1 and 4.');

coder.gpu.kernelfun();
% input signal size is 1024-by-2

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

%Changes CJ
selectedChannel_i = complex(zeros(1024, 1, 'single')); % Preallocate for fixed size
selectedChannel = complex(zeros(1024, 1, 'single')); % Preallocate for fixed size
index = 1;
for i = double(2*chan-1):double(8):double(8192)
    % selectedChannel(index) =  single(inputSig(i))/32766.0 + 1i * single(inputSig(i+1))/32766.0;
    selectedChannel(index) =  single(inputSig(i))+ 1i * single(inputSig(i+1));
    index = index + 1;
end

framePower = mean(abs(selectedChannel).^2);
selectedChannel_i = selectedChannel / sqrt(framePower);

%End Changes

inputSigI = real(selectedChannel_i);
inputSigQ = imag(selectedChannel_i);
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