# GF-GB

A scriptable GameBoy Emulator written in C.

This project is in early stages and may never be completed. But if you want to
use it anyway you can build with meson:

```sh
meson setup ./build/
meson compile -C./build/
```

You can also run the tests through meson and/or pytest. There are tests at the
bottom of most C source files. And we also build a cpython extension to run
single step tests with pytest. Meson will invoke all of these tests with:

```sh
meson test -C./build
```

If there is one single step test you want to run you can also run them directly
with pytest. For example, if you want to run the tests for the 9e opcode (which
corresponds to the fixture at `tests/sst/test_json/9e.json`):

```sh
pytest test/sst/sst_test.py::test_single_step[9e] \
  --gfgb-py-ext build/gfgb_py_testing.cpython-314-x86_64-linux-gnu.so
```

Note that you currently need to specify the location of the built python
extension with `--gfgb-py-ext` as shown above.
