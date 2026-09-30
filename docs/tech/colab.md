# Google Colab

Google Colab is the optional notebook environment for running the HELIOS training pipeline without local GPU setup. The notebook should install the pinned dependencies, fetch or generate licensed data, run the same standalone scripts as a terminal workflow, and save model, calibration, and metric artifacts.

Colab is useful for a reproducible judge demo, but it is not a hidden dependency: every stage must also work from `training/scripts/` with an explicit configuration file. Do not store credentials, raw audio, or notebook checkpoints in Git.

Notebook location: `training/helios_kws_colab.ipynb`. Requirements: `training/requirements.txt`. The retraining hook is `training/configs/*.yaml`.
