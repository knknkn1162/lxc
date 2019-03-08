# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure('2') do |config|
  # init daemon is upstart in ubuntu 14.04
  config.vm.box = 'ubuntu/trusty64'
  config.vm.synced_folder './', '/home/vagrant/shared'
  config.vm.provider 'virtualbox' do |vb|
    vb.memory = '4096'
    vb.cpus = 2
  end
  config.vm.provision 'shell', inline: <<-SHELL
    apt-get update
    # for seccomp.h and sys/capability.h
    # cgmanager is not deprecated
    apt-get install -y gcc make autotools-dev automake pkg-config libcap-dev debootstrap uidmap libseccomp-dev \
    cgmanager cgmanager-utils
  SHELL
end
