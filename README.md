
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
- Install [Yosys](https://github.com/YosysHQ/yosys) from source or from [oss-cad-suite](https://github.com/YosysHQ/oss-cad-suite-build) (a recent version is required):
- Build and install:
```sh
make
sudo make install
```


## Questions

You can ask any question you have on the [Matrix channel](https://app.element.io/#/room/#moosic-yosys-plugin:matrix.org). Don't hesitate to file an issue if you find a bug.
