# installation

```bash
# instead of `autoreconf --install`
./autogen.sh
./configure --localstatedir=/usr/local && make
# or you can check configure option by `./configure --help` command.
# if you undo `configure` operation, exec this command:
make maintainer-clean
sudo make install
```

## environments

+ Ubuntu 18.04
+ alocal 1.15.1
+ autoconf, autoheader 2.69
+ automake 1.15.1
+ make 4.1
+ gcc 7.3.0

I configured these libraries via Vagrant:

```ruby
# -*- mode: ruby -*-
# vi: set ft=ruby :
# Vagrantfile

Vagrant.configure('2') do |config|
  config.vm.box = 'ubuntu/bionic64'
  config.vm.synced_folder './', '/home/vagrant/shared'
  config.vm.provider 'virtualbox' do |vb|
    vb.memory = '2048'
    vb.cpus = 2
  end
  config.vm.provision 'shell', inline: <<-SHELL
    apt-get update
    # for seccomp.h and sys/capability.h
    apt-get install -y gcc make autotools-dev automake pkg-config libcap-dev debootstrap
    apt remove lxcfs -y --purge

  SHELL
end
```


# how to use

# preparation

```bash
sudo start-stop-daemon --stop --oknodo --user root --name lxcfs --pidfile /run/lxcfs.pid
```

## lxc-create

```bash
$ lxc-create -n debian01 -t debian -f lxc.conf
This command has to be run as root
$ lxc-create -n debian01 -t debian -f lxc.conf
Warning:
-------
Usually the template option is called with a configuration
file option too, mostly to configure the network.
eg. lxc-create -n foo -f lxc.conf -t debian
The configuration file is often:

lxc.network.type=macvlan
lxc.network.link=eth0
lxc.network.flags=up

or alternatively:

lxc.network.type=veth
lxc.network.link=br0
lxc.network.flags=up

For more information look at lxc.conf (5)

At this point, I assume you know what you do.
Press <enter> to continue ...
^Caborted
# prepare lxc.conf like the explanation above
sudo lxc-create -n debian01 -t debian -f lxc.conf
# skip..
Download complete.
Copying rootfs to /usr/local/lib/lxc/debian01/rootfs...Generating locales (this might take a while)...
Generation complete.
Root password is 'root', please change !
'debian' template installed
'debian01' created
$ sudo lxc-create -n debian02 -t debian -f lxc.conf
debootstrap is /usr/sbin/debootstrap
Checking cache download in /usr/local/cache/lxc/debian/rootfs-jessie-amd64 ...
Copying rootfs to /usr/local/lib/lxc/debian02/rootfs...Generating locales (this might take a while)...
Generation complete.
Root password is 'root', please change !
'debian' template installed
'debian02' created
```

The configuration settings are written in the several files. The detail is [here](https://gist.github.com/knknkn1162/8060d2edaa9e16882259196540ce92df).


## lxc-start


Assume that the lxc.conf is set like this:

```conf
# lxc.conf
lxc.network.type=veth
lxc.network.link=br0
lxc.network.flags=up
```

```bash
# create bridge in advance on the host(ubuntu 18.04)
$ sudo ip link add name br0 type bridge
$ sudo ip link set dev br0 up
# This configuration file if present will be used even if there is already a configuration file present in the previously created container (via lxc-create).See the link https://linuxcontainers.org/ru/lxc/manpages/man1/lxc-start.1.html
ip link show
$ sudo lxc-start --name=debian01 --  /bin/bash

# after use
$ sudo ip link delete br0 type bridge
```

### From the host

```bash
# ps auf
vagrant   3652  0.0  0.2  23160  5208 pts/0    Ss   06:07   0:00 -bash
root      4375  0.0  0.2  63972  4200 pts/0    S    06:33   0:00  \_ sudo lxc-start --name=debian03 -- /bin/bash
root      4376  0.0  0.0  10864   908 pts/0    S    06:33   0:00      \_ lxc-start --name=debian03 -- /bin/bash
root      4377  0.0  0.2  23172  5068 pts/0    S+   06:33   0:00          \_ /bin/bash
```
