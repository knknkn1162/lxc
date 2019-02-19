# installation

```bash
# instead of `autoreconf --install`
./autogen.sh
./configure && make && sudo make install
sudo mkdir -p /usr/local/var/lib/lxc
```

# environments

+ alocal 1.15.1
+ autoconf, autoheader 2.69
+ automake 1.15.1
+ make 4.1
+ gcc 7.3.0
