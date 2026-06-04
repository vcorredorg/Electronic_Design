# Lock-in Amplifier
## Overview

This repository contains the development files, documentation, firmware, and interface resources for a digital lock-in amplifier based on the open-source OLIA architecture. The project implements a compact embedded instrumentation system capable of extracting amplitude, phase, and harmonic information from weak periodic signals using synchronous detection techniques.

A lock-in amplifier is especially useful when the signal of interest is buried in noise or when conventional amplitude measurements are not sufficient. By using a known reference signal, the system can isolate the component of the measured signal that is correlated with the reference frequency. This makes the instrument suitable for experimental electronics, sensors, optical measurements, signal characterization, and academic instrumentation projects.

The implementation is built around the Teensy 4.0 microcontroller and developed using Visual Studio Code, PlatformIO, and Zephyr. In addition to the embedded firmware, the project includes a basic graphical user interface for visualizing magnitude, phase, and harmonic components, as well as modifying user-configurable acquisition and processing parameters.

## Project Purpose

The main purpose of this project is to develop a low-cost, configurable, and educational digital lock-in amplifier that can be used as an alternative to conventional laboratory instrumentation in controlled experimental environments.

Many measurement systems require detecting very small periodic signals in the presence of broadband noise, DC offsets, interference, or unwanted spectral components. Direct measurement methods may fail when the signal-to-noise ratio is too low. This project addresses that problem by implementing digital synchronous demodulation, allowing the system to recover the amplitude and phase of a signal at a known reference frequency and at selected harmonic components.

The project also aims to document the complete engineering development process, including firmware structure, signal-processing logic, hardware documents, user-interface design, requirements, testing procedures, and system validation. Therefore, the repository is intended not only as a functional implementation but also as a technical reference for understanding how a microcontroller-based lock-in amplifier can be designed, programmed, tested, and documented.

## Main Features

* Embedded signal-processing system using the Teensy 4.0 development board.
* Synchronous detection for extracting signal magnitude and phase relative to a reference signal.
* Harmonic analysis for evaluating signal components at selected multiples of the reference frequency.
* Firmware developed with PlatformIO and Zephyr inside Visual Studio Code.
* Modular firmware organization for acquisition, processing, configuration, communication, and reporting tasks.
* Basic graphical user interface for:

  * Magnitude visualization.
  * Phase visualization.
  * Harmonic visualization.
  * User-defined configuration parameters.
* Project documentation organized for academic and engineering use.
* Adaptable architecture for future improvements such as calibration, advanced filtering, data logging, communication protocols, and extended visualization tools.

## Repository Structure

The repository is organized to separate firmware, interface, hardware resources, documentation, tests, and external references. The proposed structure is shown below:

```text
.
├── README.md
├── LICENSE
├── platformio.ini
│
├── firmware/
│   ├── teensy40/
│   │   ├── src/
│   │   │   ├── main.cpp
│   │   │   ├── acquisition/
│   │   │   ├── processing/
│   │   │   ├── reference/
│   │   │   ├── harmonics/
│   │   │   ├── communication/
│   │   │   └── configuration/
│   │   ├── include/
│   │   ├── lib/
│   │   ├── boards/
│   │   └── zephyr/
│   │
│   └── README.md
│
├── interface/
│   ├── src/
│   ├── assets/
│   ├── config/
│   └── README.md
│
├── hardware/
│   ├── schematics/
│   ├── diagrams/
│   ├── wiring/
│   ├── datasheets/
│   └── README.md
│
├── docs/
│   ├── requirements/
│   ├── architecture/
│   ├── state-diagrams/
│   ├── signal-processing/
│   ├── user-interface/
│   ├── testing/
│   └── references/
│
├── tests/
│   ├── firmware/
│   ├── interface/
│   ├── signal-processing/
│   └── validation/
│
├── tools/
│   ├── scripts/
│   └── utilities/
│
└── external/
    └── OLIA/
```

Suggested directory purpose:

* `firmware/`: contains the embedded code for the Teensy 4.0, including acquisition, demodulation, harmonic calculation, configuration handling, and communication modules.
* `interface/`: contains the graphical interface used to visualize magnitude, phase, and harmonic data, and to configure user parameters.
* `hardware/`: contains connection diagrams, schematics, wiring references, datasheets, and hardware-related notes.
* `docs/`: contains the formal project documentation, including requirements, architecture, state diagrams, design explanations, and testing records.
* `tests/`: contains firmware tests, interface tests, signal-processing validation files, and experimental verification procedures.
* `tools/`: contains auxiliary scripts or utilities used for data processing, testing, configuration, or development automation.
* `external/`: may contain the original OLIA reference project, submodules, or external design references used during development.

## Technologies Used

* **Teensy 4.0**
  Main embedded processing platform. The Teensy 4.0 is based on an ARM Cortex-M7 microcontroller running at 600 MHz, which provides enough computational capability for real-time digital signal processing, harmonic calculation, communication handling, and user-configuration tasks. Its floating-point support and high clock frequency make it suitable for implementing a compact digital lock-in amplifier.

* **PlatformIO**
  Embedded development environment used to manage project configuration, dependencies, compilation, board settings, and firmware upload.

* **Zephyr**
  Real-time operating system used to structure the embedded application into manageable tasks, improve modularity, and support a more organized firmware architecture.

* **Visual Studio Code**
  Main development environment used for firmware editing, project management, compilation, and debugging workflow.

* **C / C++**
  Primary programming language for the embedded firmware, signal-processing routines, hardware abstraction, and communication logic.

* **Graphical User Interface**
  Basic interface developed to display magnitude, phase, and harmonic information, and to allow user configuration of the system.

## Reference Project

This project is based on the open-source OLIA project:

* OLIA Repository: https://github.com/ajharvie/OLIA

OLIA is an open digital lock-in amplifier designed around the Teensy 4.0. This repository uses OLIA as a technical reference and adapts its concepts to the specific goals, implementation decisions, and documentation structure of this project.
