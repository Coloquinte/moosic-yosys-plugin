
# moosic-yosys-plugin: Logic Locking for Yosys


## Using the plugin

The plugin defines a new `logic_locking` command. To run Yosys with the plugin:

```sh
yosys -m moosic-yosys-plugin
```

And in Yosys:
```
help logic_locking
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
