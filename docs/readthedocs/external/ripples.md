# Ripples & Metall

## Introduction

[Ripples](https://github.com/pnnl/ripples) is a software framework to study the Influence Maximization problem.

Ripples has a mode that uses Metall to allocate its intermediate data, which requires a large amount of memory, in storage (file system) so that
it can handle large-scale problems, exceeding DRAM capacity.

Here, we describe how to build and run Ripples with Metall.

## Build Example

Tested Environment:

- Linux
- Python 3.7
- GCC 10 (GCC >= 8.1 is required)
- CMake 3.23

The following instructions are tested with the latest version of Ripples at the time of writing (commit ID: da08b3e759642a93556f081169c61607354ecd3e).

```shell
git clone git@github.com:pnnl/ripples.git
cd ripples
git checkout da08b3e759642a93556f081169c61607354ecd3e

# Set up Python environment, if not available
# For example:
pip install --user pipenv
pip install --user conan
# If needed:
# export PATH="$HOME/.local/bin:$PATH"

pipenv --three
pipenv install
pipenv shell

# Install dependencies
conan create conan/waf-generator user/stable
conan create conan/trng user/stable
# if the line above does not work,
# conan create conan/trng libtrng/4.22@user/stable
conan create conan/metall user/stable

# Enable the Metall mode
conan install --install-folder build . -o metal=True

# Build
./waf configure --enable-metall build_release
```

## Run Example

```shell
./build/release/tools/imm --input-graph test-data/karate.tsv --seed-set-size 8 --diffusion-model LT --epsilon 0.8
```

See details [Ripples README](https://github.com/pnnl/ripples).