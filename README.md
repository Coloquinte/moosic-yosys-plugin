
# moosic-yosys-plugin: Logic Locking for Yosys


## Build as a module

To install this plugin
- Install [Yosys](https://github.com/YosysHQ/yosys) from source.
- Build and install:

```sh
make
make install
```

## Using the plugin

The plugin defines a new `logic_locking` command. To run Yosys with the plugin:

```sh
yosys -m moosic-yosys-plugin
```

And in Yosys:
```
help logic_locking
```



