# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project intends to follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned

- machine-readable JSON reports;
- regression tests using synthetic fixtures and privately held ROM hashes;
- broader Turing, Ampere, and Ada validation;
- additional read-only VBIOS tables.

## [0.2.0] - 2026-08-06

### Added

- native Windows graphical interface;
- portable command-line interface;
- GPU chip, PCI ID, and VBIOS version reporting;
- Memory Information and physical strap translation decoding;
- memory timing-map coverage classification;
- CONFIG0 through CONFIG5 timing-field decoding;
- detailed report export with source offsets;
- CMake build, install rules, and basic CLI test;
- repository documentation and GitHub continuous integration;
- project-origin notes and public references for density-mod instability.

### Changed

- replaced resize-sensitive group-box controls with stable section headings;
- coalesced GUI repaint requests and avoided redundant column resizing.
