# Preview

The image viewer of Cutefish Desktop. It opens an image with the other
images of its folder, steps through them from the keyboard, the arrows or a
swipe, and zooms and pans with the pointer, the touchpad or the keys, with
thumbnails in a sidebar and a full screen view.

## Dependencies

Preview uses cutefish-toolkit and cutefish-framework; GoogleTest for the
tests.

```sh
sudo apt install cmake libgtest-dev
```

## Build and install

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## Tests

```sh
cmake -DBUILD_TESTING=ON ..
make
dbus-run-session -- ctest --output-on-failure
```

## License

BSD-3-Clause; see [LICENSE](LICENSE).
