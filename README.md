# DMKernel - Kernel for Data Mining and AI, using DM Programming Language 

A specialized kernel for data mining and AI applications, using DM Programming Language

## Overview

DMKernel is a runtime environment that provides:

1. A command-line interface for interacting with data mining algorithms
2. A custom domain-specific language for expressing data mining operations
3. Optimized primitives for numerical and statistical computations
4. Special functions for Data Mining and AI applications
5. Using DM Programming Language - a programming language designed special for DMKernel

## Architecture

The system is organized into several components:

- **Core**: Memory management, context handling, and value system
- **Shell**: Command-line interface and interactive environment
- **Language**: Parser and interpreter for the custom language
- **Primitives**: Data mining and earthquake-specific algorithms

## Building from Source

Prerequisites:
- GCC or compatible C compiler
- Make

Build steps:

```bash
cd dmkernel
make
```

## Usage

Run the interactive shell:

```bash
./bin/dmkernel
```

Execute a script file:

```bash
./bin/dmkernel my_script.dm
```

## Command Reference

- `help` - Display available commands
- `exit` - Exit the shell
- `version` - Display kernel version information
- `run <filename>` - Execute a script file
- `exec <code>` - Execute a code snippet

## Language Reference

The DMKernel language is designed specifically for data mining tasks with a focus on earthquake aftershock prediction. It has features for:

- Matrix and vector operations
- Statistical functions and distributions
- Pattern recognition algorithms
- Signal processing
- Geospatial data handling

Example syntax:

```
# Load earthquake data
data = load_usgs("earthquake_catalog.csv")

# Extract features
features = extract_features(data)

# Train a model
model = decision_tree(features, target: "aftershock")

# Predict aftershocks
predictions = model.predict(new_data)
```

## Earthquake Prediction Features

- USGS data integration
- Temporal and spatial pattern recognition
- Magnitude analysis
- Aftershock decay modeling (Omori's law)
- Real-time monitoring capabilities

## License

This project is open source and available under the MIT License.

## Contributors

- Developed as a Data Mining course project 
