# ESP-IDF Examples

Welcome to the **ESP-IDF Examples** repository! This collection contains multiple **fully buildable and standalone** ESP-IDF projects, each demonstrating a different feature or functionality of the ESP32 platform. Each example is structured as an independent ESP-IDF project, making it easy to navigate, build, and flash.

## Repository Structure

```
ESP-IDF-Examples/
├── example1/
│   ├── CMakeLists.txt
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── example1.c
│── example2/
│   ├── CMakeLists.txt
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── example2.c
├── .vscode/  (VS Code settings)
├── .devcontainer/  (Dev container setup)
└── README.md  (This file)
```

## How to Use
Each folder inside this repository represents a fully independent ESP-IDF project. To build and flash an example:

1. Open a terminal and navigate to the desired example:
   ```sh
   cd example1/

2. Set the ESP32 target (only needed once per session):
   ```sh
   idf.py set-target esp32

3. Build, flash, and monitor the output:
   ```sh
   idf.py build flash monitor

## Requirements

* ESP-IDF installed and configured.

* A compatible ESP32 board.

* A serial connection to the board for flashing and debugging.

## VS Code Integration

This repository includes .vscode/ settings for easier development. If you're using VS Code:

* Open this repository in VS Code.

* Use the ESP-IDF terminal for running build and flash commands.

* Ensure the ESP-IDF extension is installed for best experience.

## Dev Container Support

If you're using Dev Containers, this repository includes a `.devcontainer/` folder with a pre-configured environment for ESP-IDF development. Simply open the repository in a Dev Container to get started.

## Contributing

Feel free to contribute additional examples! Make sure each example:

* Is placed in its own folder as a standalone project.

* Includes a `CMakeLists.txt` file in the root and `main/` folder.

* Follows ESP-IDF project structure.

## License

This repository is licensed under the MIT License. See `LICENSE` for details.

🚀 Happy coding with ESP32!
