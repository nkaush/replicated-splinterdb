# replicated-splinterdb

## Building from Scratch

```bash
export CC=clang
export LD=clang
export CXX=clang++
git clone https://github.com/nkaush/replicated-splinterdb.git
cd replicated-splinterdb/
git submodule update --init --recursive
sudo ./setup_server.sh
mkdir build && cd build
cmake .. && make -j `nproc` all spl-server spl-client
```

## Development Process

Run `make dev` in a shell to start a container with all the necessary dependencies to build this project. This make rule will also mount the `src/`, `apps/`, and `include/` directories onto the `/work` directory in the container. The `libnuraft` and `libsplinterdb` libraries will be built during the container image build stage.

The container will start in a shell in the `/work/build` directory. Run `./build` to build the source code. You can make chanegs to any files in the `src/`, `apps/`, and `include/` directories and recompile them without exiting and restarting the container. Simply make changes and run `./build` again to recompile.

## TODOs

- [x] Track key-based miss rates in splinterdb
- [ ] Intelligent thread pool sizing
- [ ] Detailed latency breakdowns
- [ ] clang-tidy
- [x] Switch over to protobuf or some async networking library and use nuraft::async_handler
- [ ] YAML config parsing (see [yaml-cpp](https://github.com/jbeder/yaml-cpp/wiki/Tutorial))

# Starting:
```
./build/apps/spl-server -serverid 1 -bind all -raftport 10000 -joinport 10001 -clientport 10002

./build/apps/spl-server -serverid 2 -bind all -raftport 10003 -joinport 10004 -clientport 10005 -seed localhost:10001

./build/apps/spl-server -serverid 3 -bind all -raftport 10006 -joinport 10007 -clientport 10008 -seed localhost:10001
```
