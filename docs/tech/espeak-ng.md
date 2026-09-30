# espeak-ng

espeak-ng is a small, deterministic, offline text-to-speech engine used to generate training positives. It is not natural sounding, but phonetic diversity is more important than naturalness for initial keyword spotting.

HELIOS varies eight voices, six speeds (110–210 WPM), five pitches, and four amplitudes: `8 × 6 × 5 × 4 = 960` positives.

```bash
espeak-ng -v en-us -s 150 -p 60 -a 160 -w out.wav "HELIOS"
```

Synthetic speech can sound robotic and may not generalize perfectly. SNR mixing, reverb, speed, gain, and a later real-audio fine-tuning set mitigate that limitation. The tool is GPL-3.0; check distribution obligations for tool packaging, while generated audio should be reviewed for the applicable installation and source terms.

See `training/scripts/generate_positives.py`.
