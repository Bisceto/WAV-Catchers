## Initialization
## Install libraries : librosa torch torchvision torchaudio pydub
# conda install -c pytorch pytorch
# conda install -c pytorch torchvision
# conda install -c pytorch torchaudio 
# conda install -c conda-forge pydub

import numpy as np # linear algebra
import pandas as pd # data processing, CSV file I/O (e.g. pd.read_csv)

import librosa
import librosa.display
import matplotlib.pyplot as plt
import IPython.display as ipd
import os

import torch
from torch import nn
from torch.utils.data import Dataset, DataLoader
import random
from transformers import HubertModel, Wav2Vec2FeatureExtractor

from pydub import AudioSegment
from pydub.silence import split_on_silence

from flask import Flask, request, render_template, Response
import urllib

MIN_AUDIO_LEN = 0.6 # for splicing audio into 4 parts
MIN_THRESH = -40    # for splicing into 4 parts
device = 'cpu'
esp32_ip = '192.168.39.161' # esp32 ip address
filename = "recording.wav"

# helper functions

# load wav file and return spectrogram
def wav2melSpec(AUDIO_PATH):
    audio, sr = librosa.load(AUDIO_PATH)
    return librosa.feature.melspectrogram(y=audio, sr=sr)

#plot spectrogram
def imgSpec(ms_feature):
    fig, ax = plt.subplots()
    ms_dB = librosa.power_to_db(ms_feature, ref=np.max)
    print(ms_feature.shape)
    img = librosa.display.specshow(ms_dB, x_axis='time', y_axis='mel', ax=ax)
    fig.colorbar(img, ax=ax, format='%+2.0f dB')
    ax.set(title='Mel-frequency spectrogram');

# load and hear audio
def hear_audio(AUDIO_PATH):
    audio, sr = librosa.load(AUDIO_PATH)
    
    print("\t", end="")
    ipd.display(ipd.Audio(data=audio, rate=sr))
       
def get_audio_info(path, show_melspec=False, label=None):
    spec = wav2melSpec(path)
    if label is not None:
        print("Label:", label)
    if show_melspec is not False:
        imgSpec(spec)
    hear_audio(path)

# load wav file and return np array and sampling rate
def load_audio(AUDIO_PATH):
    audio, sr = librosa.load(AUDIO_PATH)
    return audio, sr

## Handle the testing audio
## Input to model should have similar sample rate and length
def resample(sample, sample_rate, new_sample_rate):
    return librosa.resample(sample, orig_sr=sample_rate, target_sr=16000)

def pad(sample, desired_length=16000):
    # Pad the audio tensor with zeros to a fixed length of 16000*1s
    if len(sample) < desired_length:
        padding = desired_length - len(sample)
        sample = np.pad(sample, (0, padding), 'constant')
    elif len(sample) > desired_length:
        sample = sample[:desired_length]
    return sample

# Possible bug: Somehow, if we load torch first the kernel dies due to lack of ram
# Solved for now
test = 'recording.wav'
spec = wav2melSpec(test)
print("Librosa initialization ok")

model_id = "facebook/hubert-large-ls960-ft"
feature_extractor = Wav2Vec2FeatureExtractor.from_pretrained(model_id)
hubert_base = HubertModel.from_pretrained(model_id)
class HubertAudioModel(torch.nn.Module):
    def __init__(self, hubert_model=hubert_base):
        super().__init__()
        self.hubert = hubert_model
        self.fc1 = torch.nn.Linear(49*1024, 256)
        self.fc2 = torch.nn.Linear(256, 10)

    def forward(self, audio_array):
        # Resample the audio to the required sample rate (16kHz for Hubert)
        # audio_array = librosa.load(audio_file, sr=16000, mono=False)[0]
        # print(f"audio_array shape before Wav2Vec: {audio_array.shape}")
        input = feature_extractor(audio_array, 
                           sampling_rate=16000,
                           padding=True, 
                           return_tensors='pt').to(device)
        
        # print(f"input.input_values shape after Wav2Vec: {input.input_values.shape}")

        input = input.input_values.squeeze(dim=0)
        # print(f"input shape after squeeze: {input.shape}")

        # Pass the spectrogram through the Hubert model
        output = self.hubert(input)
        # print(f"output.last_hidden_state shape after hubert: {output.last_hidden_state.shape}")

        # Flatten the output of the Hubert model
        output = torch.flatten(output.last_hidden_state, start_dim=1)

        # print(f"output shape after flatten: {output.shape}")

        # Pass the flattened output through two dense layers
        output = torch.nn.functional.relu(self.fc1(output))
        output = self.fc2(output)

        return output

def make_predictions(model, data, device=device):
    pred_probs = []
    model.eval()
    with torch.inference_mode():
        for sample in data:
            # Prepare the sample (add a batch dimension and pass to target device)
            sample = torch.unsqueeze(sample, dim=0).to(device)

            # Forward pass (model outputs raw logits)
            pred_logits = model(sample)

            # Get prediction probability (logit -> prediction probability)
            pred_prob = torch.softmax(pred_logits.squeeze(), dim=0)

            # Get pred_probs off the GPU for further calculations
            pred_probs.append(pred_prob.cpu())
    
    # Stack the pred_probs to turn list into a tensor
    return torch.stack(pred_probs)

# Model initialization
model = HubertAudioModel().to(device)
model.load_state_dict(torch.load("model_hf.pth"))
print("All keys matched successfully")
print("Hubert Model initalization ok\n")

print("Evaluation mode for Hubert model ")
model.eval()

# Splice audio into 4 parts (4 numbers)
def audio_len_check(audio_chunks):
    for chunk in audio_chunks:
        if chunk.duration_seconds < MIN_AUDIO_LEN:
            return False
    return True

def splice_4parts(filename):
    sound_file = AudioSegment.from_wav(filename)
    chunks = 0
    silence_len = 200
    threshold = -20
    clear = True
    
    while True:  # adjust parameters until 4 slices are achieved
    # We limit grid search to  0.025s <= silence_len <= 0.2s , MIN_THRESH dBFS <= threshold <= - 20dBFS
        if threshold < MIN_THRESH:
            if silence_len == 100:
                print("Audio file Unclear... Please retry")
                clear = False
                break
            silence_len = max(100,silence_len-25)
            threshold = -20
        audio_chunks = split_on_silence(sound_file, 
            # must be silent for at least x millisecond
            min_silence_len=silence_len,

            # consider it silent if quieter than y dBFS
            silence_thresh=threshold,

            # keep 300 ms of leading/trailing silence                            
            keep_silence=300
        )
        chunks = len(audio_chunks)
        threshold -= 1
        print(silence_len,threshold)
        if chunks == 4 and audio_len_check(audio_chunks):
            break
        
    
    # output 4 wav files for classification
    for i, chunk in enumerate(audio_chunks):

        out_file = f"output_{i}.wav"
        print ("exporting", out_file)
        chunk.export(out_file, format="wav")
    return clear

def make_predictions_all():
    test_samples = []
    for i in range(0,4):
        sample_path =  f'output_{i}.wav'
        print(f'Adding {sample_path} to test sample')
        sample, sample_rate = load_audio(sample_path)
        if sample_rate != 16000:
            sample = resample(sample, sample_rate, 16000)
        sample = pad(sample)
        sample_tensor = torch.from_numpy(sample)
        #     print(sample_tensor.shape)
        test_samples.append(sample_tensor)
        #test_labels.append(label)
    pred_probs = make_predictions(model=model,data=test_samples)
    pred_classes = pred_probs.argmax(dim=1)
    pred_classes = pred_classes.tolist()
    return pred_classes


# Start Flask server
app = Flask(__name__)
# Create just a single route to read data from our ESP32
@app.route('/prompt', methods = ['GET'])
def addData():
    ''' The one and only route. It extracts the
    data from the request, converts to float if the
    data is not None, then calls the callback if it is set
    '''
    global _callback_
    
    datastr = request.args.get('data') 

    print("\nData from ESP32: ", datastr, "\n")
    if datastr == 'yes':
        urllib.request.urlretrieve(f"http://{esp32_ip}/recording.wav", filename=filename) # Download wav file
        success = splice_4parts(filename)
        print(success)
        if success:
            preds = make_predictions_all()
            print(preds)
            return f"Classification: {preds}", 200
        else:
            return f"Unclear Audio, try again", 200

def main():
    app.run(host = "0.0.0.0", port = '3237')


if __name__ == '__main__':
    main()
