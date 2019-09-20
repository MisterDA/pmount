# pmount

Mount removable devices as normal user.

``` shell
meson build

# See build options in `meson_options.txt`.
meson configure build -Dcryptsetup-prog='/usr/bin/cryptsetup'

ninja -C build
DESTDIR=... ninja -C build install	# for packagers
```
