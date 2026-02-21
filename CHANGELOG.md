# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Added
* Support S/PDIF output (I2S + S/PDIF, I2S and S/PDIF selection)
* Support Hi-Res audio up to 24bit 192KHz
  * Support 24bit resolution for WAV and FLAC
  * Support sampling frequencies up to 192KHz for WAV and FLAC
* Support sampling frequencies up to 48Hz for MP3
* Display icon to indicate bit resolution and sampling frequency
* Display codec icon
* Support ID3 tag for WAV
### Changed
* Change icon of audio files in FileView
### Fixed
* Fix track number display for WAV case
* Fix FileView icon to apply sorted order

## [v0.9.0] - 2026-02-17
### Added
* Add CI workflow with GitHub Actions
### Changed
* Permit MP3 decode error up to 2 times before abort
* Clear button_event in draw() & entry() to improve button action feeling
* Use average bitrate estimated by past accumulated bitrate to suppress flickering total time (in case of VBR w/o Xing header)
### Fixed
* Fixed btn_ent clear timing
