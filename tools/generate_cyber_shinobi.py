#!/usr/bin/env python3
"""
MacheteSVG - Cyber Shinobi Soundtrack Generator
Synthesizes a high-fidelity 48000 Hz stereo lo-fi/cyberpunk/ninja background track
inspired by Cyberpunk 2077 and Naruto (electronic synth bass + Japanese pentatonic flute and koto).
Encodes directly to OGG Vorbis with seamless looping tags.
"""

import math
import struct
import wave
import subprocess
import os

SAMPLE_RATE = 48000
BPM = 112.0
BEAT_DUR = 60.0 / BPM
BAR_DUR = BEAT_DUR * 4.0
NUM_BARS = 64
TOTAL_DURATION = BAR_DUR * NUM_BARS
TOTAL_SAMPLES = int(SAMPLE_RATE * TOTAL_DURATION)

print(f"Generating Cyber Shinobi track: {NUM_BARS} bars @ {BPM} BPM = {TOTAL_DURATION:.2f} seconds ({TOTAL_SAMPLES} samples)")

# Allocate stereo buffers
left_channel = [0.0] * TOTAL_SAMPLES
right_channel = [0.0] * TOTAL_SAMPLES

# Frequencies in D minor / Insen Japanese Scale (D, Eb, G, A, C)
# D2=73.42, Eb2=77.78, G2=98.00, A2=110.00
# D3=146.83, Eb3=155.56, G3=196.00, A3=220.00, C4=261.63
# D4=293.66, Eb4=311.13, G4=392.00, A4=440.00, C5=523.25, D5=587.33
NOTES = {
    'D2': 73.42, 'Eb2': 77.78, 'F2': 87.31, 'G2': 98.00, 'A2': 110.00, 'Bb2': 116.54, 'C3': 130.81,
    'D3': 146.83, 'Eb3': 155.56, 'F3': 174.61, 'G3': 196.00, 'A3': 220.00, 'Bb3': 233.08, 'C4': 261.63,
    'D4': 293.66, 'Eb4': 311.13, 'F4': 349.23, 'G4': 392.00, 'A4': 440.00, 'Bb4': 466.16, 'C5': 523.25,
    'D5': 587.33, 'Eb5': 622.25, 'F5': 698.46, 'G5': 783.99, 'A5': 880.00, 'C6': 1046.50
}

# 1. CYBERPUNK SYNTH BASS (Saw + Sub with Lowpass filter sweep)
bass_chords = [
    ('D2', 'D3'), ('D2', 'D3'), ('Bb2', 'Bb3'), ('A2', 'A3'),
    ('D2', 'D3'), ('D2', 'D3'), ('G2', 'G3'), ('A2', 'A3'),
    ('D2', 'D3'), ('Eb2', 'Eb3'), ('Bb2', 'Bb3'), ('A2', 'A3'),
    ('D2', 'D3'), ('G2', 'G3'), ('Bb2', 'Bb3'), ('A2', 'A3'),
]

for bar in range(NUM_BARS):
    chord = bass_chords[bar % len(bass_chords)]
    f_sub = NOTES[chord[0]]
    f_saw = NOTES[chord[1]]
    bar_start_sample = int(bar * BAR_DUR * SAMPLE_RATE)
    bar_samples = int(BAR_DUR * SAMPLE_RATE)
    
    # 16th-note pumping bassline
    subdivisions = 16
    sub_samples = bar_samples // subdivisions
    for s in range(subdivisions):
        sub_start = bar_start_sample + s * sub_samples
        sub_len = min(sub_samples, TOTAL_SAMPLES - sub_start)
        # Accent on 1, 4, 7, 10, 13
        accent = 1.2 if (s % 3 == 0) else 0.85
        if s % 4 == 0:
            accent = 1.4
            
        for i in range(sub_len):
            idx = sub_start + i
            if idx >= TOTAL_SAMPLES:
                break
            t_env = i / sub_samples
            env = math.exp(-3.5 * t_env) * accent
            # Sub bass sine + saturated saw
            sub = math.sin(2 * math.pi * f_sub * (i / SAMPLE_RATE)) * 0.45
            saw = (2.0 * ((f_saw * (i / SAMPLE_RATE)) % 1.0) - 1.0)
            # Lowpass / saturation
            saw_filtered = math.tanh(saw * 1.5) * 0.25
            sample_val = (sub + saw_filtered) * env * 0.35
            
            left_channel[idx] += sample_val
            right_channel[idx] += sample_val

# 2. ELECTRONIC DRUMS (808 Kick, Snare, Cyber Hi-hats)
for bar in range(NUM_BARS):
    bar_start = int(bar * BAR_DUR * SAMPLE_RATE)
    
    # Kicks on beat 1, 2.5, 3.5
    kick_times = [0.0, 1.5 * BEAT_DUR, 2.5 * BEAT_DUR] if (bar % 4 != 3) else [0.0, 1.0 * BEAT_DUR, 2.0 * BEAT_DUR, 3.0 * BEAT_DUR]
    for kt in kick_times:
        k_start = bar_start + int(kt * SAMPLE_RATE)
        k_len = int(0.35 * SAMPLE_RATE)
        for i in range(min(k_len, TOTAL_SAMPLES - k_start)):
            idx = k_start + i
            t = i / SAMPLE_RATE
            # Pitch drop from 150Hz to 45Hz
            pitch = 45.0 + 105.0 * math.exp(-35.0 * t)
            phase = 2 * math.pi * (45.0 * t - (105.0 / 35.0) * (math.exp(-35.0 * t) - 1.0))
            amp = math.exp(-9.0 * t)
            # Click transient
            click = (1.0 if i < 150 else 0.0) * math.sin(2 * math.pi * 1200.0 * t) * 0.3
            val = (math.sin(phase) + click) * amp * 0.55
            left_channel[idx] += val
            right_channel[idx] += val

    # Snares on beat 2 and 4
    snare_times = [1.0 * BEAT_DUR, 3.0 * BEAT_DUR]
    for st in snare_times:
        s_start = bar_start + int(st * SAMPLE_RATE)
        s_len = int(0.28 * SAMPLE_RATE)
        for i in range(min(s_len, TOTAL_SAMPLES - s_start)):
            idx = s_start + i
            t = i / SAMPLE_RATE
            tone = math.sin(2 * math.pi * 185.0 * t) * math.exp(-22.0 * t) * 0.35
            # White noise burst
            noise = ((hash((idx, 42)) % 2000) / 1000.0 - 1.0) * math.exp(-14.0 * t) * 0.35
            val = (tone + noise) * 0.45
            left_channel[idx] += val * 0.95
            right_channel[idx] += val * 1.05

    # 16th-note Hi-hats with stereo panning
    for step in range(16):
        ht = step * (BEAT_DUR / 4.0)
        h_start = bar_start + int(ht * SAMPLE_RATE)
        is_open = (step % 4 == 2 and bar % 2 == 1)
        h_len = int((0.18 if is_open else 0.045) * SAMPLE_RATE)
        pan = 0.3 if (step % 2 == 0) else -0.3
        vol = (0.22 if is_open else 0.12) * (1.2 if (step % 4 == 0) else 0.8)
        for i in range(min(h_len, TOTAL_SAMPLES - h_start)):
            idx = h_start + i
            t = i / SAMPLE_RATE
            decay = math.exp((-18.0 if is_open else -65.0) * t)
            noise = ((hash((idx, 107)) % 2000) / 1000.0 - 1.0)
            # Metallic high-pass
            metal = (noise - 0.7 * math.sin(2 * math.pi * 8000.0 * t)) * decay * vol
            left_channel[idx] += metal * (1.0 - pan)
            right_channel[idx] += metal * (1.0 + pan)

# 3. JAPANESE KOTO / SHAMISEN PLUCKS (Naruto/Ninja Insen Arpeggios)
koto_pattern = [
    'D4', 'A4', 'C5', 'D5', 'Eb5', 'D5', 'C5', 'A4',
    'G4', 'A4', 'C5', 'Eb5', 'D5', 'C5', 'A4', 'G4',
    'D4', 'G4', 'A4', 'C5', 'D5', 'C5', 'A4', 'G4',
    'Eb4', 'G4', 'A4', 'D5', 'C5', 'A4', 'G4', 'Eb4'
]

for bar in range(NUM_BARS):
    bar_start = int(bar * BAR_DUR * SAMPLE_RATE)
    # Koto plays 16th-note arpeggio
    for n_idx in range(16):
        note_name = koto_pattern[(bar * 16 + n_idx) % len(koto_pattern)]
        freq = NOTES[note_name]
        start_samp = bar_start + int(n_idx * (BEAT_DUR / 4.0) * SAMPLE_RATE)
        dur_samp = int(0.35 * SAMPLE_RATE)
        pan = 0.45 * math.sin(n_idx * 0.8)
        for i in range(min(dur_samp, TOTAL_SAMPLES - start_samp)):
            idx = start_samp + i
            t = i / SAMPLE_RATE
            # String pluck envelope (sharp attack, exponential decay)
            env = math.exp(-12.0 * t)
            # String harmonics (fundamental + 2nd + 3rd harmonic)
            harmonic1 = math.sin(2 * math.pi * freq * t)
            harmonic2 = math.sin(2 * math.pi * (freq * 2) * t) * 0.45
            harmonic3 = math.sin(2 * math.pi * (freq * 3) * t) * 0.2
            harmonic4 = math.sin(2 * math.pi * (freq * 4) * t) * 0.1
            pluck = (harmonic1 + harmonic2 + harmonic3 + harmonic4) * env * 0.18
            
            left_channel[idx] += pluck * (1.0 - pan)
            right_channel[idx] += pluck * (1.0 + pan)

# 4. SHAKUHACHI / NINJA FLUTE LEAD (Naruto Battle/Sadness Theme Motif)
flute_melody = [
    # (Bar, Beat, DurationInBeats, Note)
    (0, 0.0, 3.0, 'D5'), (0, 3.0, 1.0, 'Eb5'),
    (1, 0.0, 2.0, 'D5'), (1, 2.0, 2.0, 'A4'),
    (2, 0.0, 3.0, 'C5'), (2, 3.0, 1.0, 'D5'),
    (3, 0.0, 4.0, 'A4'),
    (4, 0.0, 2.5, 'D5'), (4, 2.5, 1.5, 'G5'),
    (5, 0.0, 2.0, 'F5'), (5, 2.0, 2.0, 'Eb5'),
    (6, 0.0, 3.0, 'D5'), (6, 3.0, 1.0, 'C5'),
    (7, 0.0, 4.0, 'D5'),
    # Climax section
    (8, 0.0, 2.0, 'A5'), (8, 2.0, 1.5, 'G5'), (8, 3.5, 0.5, 'F5'),
    (9, 0.0, 2.0, 'G5'), (9, 2.0, 2.0, 'D5'),
    (10, 0.0, 1.5, 'Eb5'), (10, 1.5, 1.5, 'D5'), (10, 3.0, 1.0, 'C5'),
    (11, 0.0, 4.0, 'A4'),
    (12, 0.0, 2.0, 'D5'), (12, 2.0, 2.0, 'Eb5'),
    (13, 0.0, 2.0, 'G5'), (13, 2.0, 2.0, 'A5'),
    (14, 0.0, 3.0, 'C6'), (14, 3.0, 1.0, 'A5'),
    (15, 0.0, 4.0, 'D5'),
]

for rep in range(max(1, NUM_BARS // 16)):
    for item in flute_melody:
        bar_offset, beat, dur_beats, note_name = item
        bar = bar_offset + rep * 16
        f_base = NOTES[note_name]
        start_sample = int((bar * BAR_DUR + beat * BEAT_DUR) * SAMPLE_RATE)
        note_dur = dur_beats * BEAT_DUR
        dur_samples = int(note_dur * SAMPLE_RATE)
    
    for i in range(min(dur_samples, TOTAL_SAMPLES - start_sample)):
        idx = start_sample + i
        t = i / SAMPLE_RATE
        # Envelope: gentle attack, sustain, smooth release
        att = min(1.0, t / 0.08)
        rel = min(1.0, (note_dur - t) / 0.12) if (t > note_dur - 0.12) else 1.0
        env = att * rel
        
        # Pitch vibrato (5.5 Hz, starts after 0.2s)
        vib_delay = min(1.0, max(0.0, (t - 0.2) / 0.3))
        vibrato = math.sin(2 * math.pi * 5.5 * t) * (f_base * 0.018) * vib_delay
        freq = f_base + vibrato
        
        # Shakuhachi breath noise
        breath = ((hash((idx, 333)) % 2000) / 1000.0 - 1.0) * 0.08
        
        # Flute waveform (sine with soft odd harmonics)
        phi = 2 * math.pi * freq * t
        flute_wave = math.sin(phi) + 0.25 * math.sin(2 * phi) + 0.12 * math.sin(3 * phi)
        sample_val = (flute_wave + breath) * env * 0.26
        
        # Stereo delay / reverb reflection
        left_channel[idx] += sample_val * 0.95
        right_channel[idx] += sample_val * 1.05
        # Reverb echo after 0.28s
        echo_idx = idx + int(0.28 * SAMPLE_RATE)
        if echo_idx < TOTAL_SAMPLES:
            left_channel[echo_idx] += sample_val * 0.22
            right_channel[echo_idx] += sample_val * 0.18

# 5. MASTERING: Soft Clipper and Normalization
max_peak = 0.001
for i in range(TOTAL_SAMPLES):
    max_peak = max(max_peak, abs(left_channel[i]), abs(right_channel[i]))

import shutil

norm_factor = 1.05 / max_peak if max_peak > 0.0 else 1.0
print(f"Mastering: peak = {max_peak:.3f}, normalization factor = {norm_factor:.3f}")

raw_pcm = bytearray()
for i in range(TOTAL_SAMPLES):
    # Soft saturation curve
    l = math.tanh(left_channel[i] * norm_factor)
    r = math.tanh(right_channel[i] * norm_factor)
    s16_l = int(max(-32767, min(32767, l * 32767)))
    s16_r = int(max(-32767, min(32767, r * 32767)))
    raw_pcm.extend(struct.pack('<hh', s16_l, s16_r))

wav_tmp = "/tmp/cyber_shinobi_master.wav"
with wave.open(wav_tmp, 'wb') as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(raw_pcm)

print(f"Master WAV exported: {wav_tmp} ({len(raw_pcm)} bytes)")

# 6. ENCODE TO OGG VORBIS WITH FFPEG AND SEAMLESS LOOPING METADATA
out_ogg = "assets/audio/music/cyber-shinobi.ogg"
ffmpeg_bin = shutil.which("ffmpeg") or "/Users/thebigmike/.local/bin/ffmpeg"
cmd = [
    ffmpeg_bin,
    "-y",
    "-i", wav_tmp,
    "-c:a", "libvorbis",
    "-q:a", "6", # High quality Vorbis (~192 kbps)
    "-ar", "48000",
    "-metadata", f"TITLE=Cyber Shinobi 2077 (MacheteSVG Theme)",
    "-metadata", f"ARTIST=MacheteSVG Team",
    "-metadata", f"LOOPSTART=0",
    "-metadata", f"LOOPLENGTH={TOTAL_SAMPLES}",
    out_ogg
]

subprocess.run(cmd, check=True)
print(f"Successfully generated: {out_ogg}")
if os.path.exists(wav_tmp):
    os.remove(wav_tmp)
