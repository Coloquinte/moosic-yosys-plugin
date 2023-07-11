
# Logic Locking for Yosys


## Using the plugin

The plugin defines a new `logic_locking` command. To run Yosys with the plugin:

```sh
yosys -m moosic-yosys-plugin
```

And in Yosys, with a synthetized design:
```
# Look at the command documentation
help logic_locking

# Add logic locking up to 5% of the module size, maximizing output corruption
logic_locking -max-percent 5 -target corruption
```


## Installation instructions

To install this plugin
- Install [Yosys](https://github.com/YosysHQ/yosys) from source (a recent version is required).
```sh
git clone https://github.com/YosysHQ/yosys
cd yosys
# For global install
sudo make install
# Alternatively for a user install
make install PREFIX=~/.local
export PATH=~/.local/bin/:$PATH
```

- Build and install:
```sh
make
sudo make install
```
