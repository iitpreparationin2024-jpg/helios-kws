# ESC-50 Environmental Sound Dataset

ESC-50 contains 2,000 labeled five-second clips across 50 environmental classes. HELIOS uses suitable classes such as fan, traffic, rain, wind, crowd, music, television, and machinery as a noise pool for augmentation.

The workflow downloads the archive, selects a documented subset (for example 150 clips), and mixes it with keyword audio at SNRs from -5 to +20 dB. ESC-50 is CC-BY-NC-3.0, so it is suitable for research and hackathons but not unrestricted commercial redistribution.

When there is no internet, `download_esc50.py` can provide synthetic white, brown, hum, tone, and amplitude-modulated noise. See `training/scripts/download_esc50.py` and `training/scripts/augment.py`.
