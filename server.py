import cv2
import cvlib as cv
from cvlib.object_detection import draw_bbox
import paho.mqtt.client as mqtt
from time import *
import urllib.request
import numpy as np

import numpy as np # linear algebra
import pandas as pd # data processing, CSV file I/O (e.g. pd.read_csv)

import librosa
import librosa.display
import matplotlib.pyplot as plt
import os
from datetime import date,datetime

import torch
from torch import nn
from torch.utils.data import Dataset, DataLoader
import random
from transformers import HubertModel, Wav2Vec2FeatureExtractor

from pydub import AudioSegment
from pydub.silence import split_on_silence

import paho.mqtt.client as mqtt

password_audio_filename : str = 'recording.wav'
attempts_left : int = 2
client = mqtt.Client()

MIN_AUDIO_LEN = 0.6 # for splicing audio into 4 parts, minimum audio length of each splice is 600ms
MIN_THRESH = -30    # for splicing into 4 parts, amplitude the algorithm considers as silence
PASSWORD = 5678
START_FILE = True   # To create new audiofile for new recording attempt
NUM_DIGITS = 4      # Digits of password
device = 'cpu'
# esp32_ip = '192.168.39.161' # esp32 ip address
password_audio_filename : str = 'recording.wav'
audio_filepath = ''
current_dir = os.getcwd()   # get current working directory

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
#test = 'recording.wav'
#spec = wav2melSpec(test)
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

            # Get pred_probs off the CPU for further calculations
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

def splice_parts(filename):
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
            silence_len = max(100,silence_len-50)
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
        if chunks == NUM_DIGITS and audio_len_check(audio_chunks):
            break
    
    # output 4 wav files for classification
    for i, chunk in enumerate(audio_chunks):
        out_file = f"output_{i}.wav"
        output_dir = os.path.join(current_dir,'recordings','temporary_files',out_file)
        print ("exporting", out_file)
        chunk.export(output_dir, format="wav")
    return clear

def make_predictions_all():
    test_samples = []
    for i in range(0,NUM_DIGITS):
        sample_path =  f'output_{i}.wav'
        sample_path = os.path.join(current_dir,'recordings','temporary_files',sample_path)
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

def on_connect(client, userdata, flags, rc):
    print("Connected with result code: " + str(rc))
    client.subscribe("sensors/#")
    client.subscribe("telegram/#")

def on_message(client, userdata, message):
    global attempts_left
    global START_FILE
    global audio_filepath
    topic = message.topic
    data = message.payload
    timestamp = time()
    
    if topic.startswith("sensors/motion"):
        handle_motion()
    elif topic.startswith("sensors/audio"):
        save_audio_file(topic, data, timestamp)
    # elif topic == "outside_board/reset_attempts":
    #     print(data)
    #     if data == '1':
    #         print('Resetting attempts')
    #         attempts_left = 3
    elif topic == "telegram/command":
        handle_telegram_command(client, data.decode())
    elif topic.startswith("sensors/microphone/snippet"):
        now = datetime.now()
        # dd/mm/YY H:M:S
        if START_FILE: # Create new file for each recording
            dt_string = now.strftime("%d-%m-%Y_%HH%MM%SS")
            recording_filename_datetime = 'recording-' + dt_string + '.wav'
            audio_filepath = os.path.join(current_dir,'recordings',recording_filename_datetime)
            START_FILE = False

        save_audio_snippet(message.payload,audio_filepath)
        print("Audio message received of length", len(message.payload))
    elif topic.startswith("sensors/microphone/recording_finished"):
        START_FILE = True
        #is_microphone_recording = False
        print("Microphone recording finished, Starting Audio classification")
        success = splice_parts(audio_filepath)
        if success:
            preds = make_predictions_all()
            strpreds = [str(i) for i in preds]
            # strpreds = ['5','6','7','8'] # Test for correct password
            if ''.join(strpreds) == str(PASSWORD):  # Correct password
                print('Correct Password')
                client.publish("outside_board/correct_password_attempt", "Correct")
                client.publish("actuators/lcd/correct_password_attempt", "Correct") # Hardcoded at ESP32 side
            else:                                   # Wrong password
                client.publish("actuators/lcd/display_message", "Wrong Password!\n" + '-'.join(strpreds))
                attempts_left = max(attempts_left - 1, 0)
                print(f"Attemps left: {attempts_left}")
                client.publish("actuators/lcd/wrong_password_attempt", str(attempts_left))
            print(preds)
        else:                                       # Unclear audio 
            print("Unclear audio, try again")
            attempts_left = max(attempts_left - 1, 0)
            print(f"Attemps left: {attempts_left}")
            client.publish("actuators/lcd/wrong_password_attempt", str(attempts_left))
        if attempts_left == 0:
            print("No attempts left")
            client.publish("outside_board/wrong_password_attempt","1")  # 1 signifies no attemps left

    elif topic.startswith("sensors/microphone/recording_started"):
        #is_microphone_recording = True
        clear_recording()
        print("Microphone has started recording")

    print("Received message")

def clear_recording():
    open(password_audio_filename, 'w').close()

def save_audio_snippet(message_payload : str,audio_filepath):
    f = open(audio_filepath, 'ab')
    f.write(message_payload)
    f.close()

def save_audio_file(topic, data, timestamp):
    filename = topic.split("/")[-1] + str(timestamp)
    filepath = "./audio/" + filename
    with open(filepath, "wb") as audio_file:
        audio_file.write(data)

def handle_telegram_command(client, command):
    global attempts_left
    if command == "unlock":
        client.publish("arduino/command", "unlock")
    elif command == "lock":
        client.publish("arduino/command", "lock")
    elif command == 'reset':
        print('Resetting attempts')
        attempts_left = 3

def handle_motion():
    url = 'http://172.20.10.6/cam-hi.jpg'
    labels = []
    for i in range(10): #10 frames
        img_resp=urllib.request.urlopen(url)
        imgnp=np.array(bytearray(img_resp.read()),dtype=np.uint8)
        im = cv2.imdecode(imgnp,-1)
        bbox, label, conf = cv.detect_common_objects(im, model="yolov3")
        output_image = draw_bbox(im, bbox, label, conf)
        
        for item in label:
            if item in labels:
                pass
            else:
                labels.append(item)
                
        cv2.imshow("At your door", output_image)
        
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break
    cv2.destroyWindow("At your door")
    print(labels)
    if "person" in labels:
        client.publish("detection/camera", "Person detected")

client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)
client.loop_forever() #starts running loop in background to communicate with MQTT broker
