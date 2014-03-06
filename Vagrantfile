# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # Every Vagrant virtual environment requires a box to build off of.
  config.vm.box = "precise64"
  config.vm.box_url = "http://files.vagrantup.com/precise64.box"
  config.vm.hostname = "fpti"
  config.ssh.forward_agent = true

  config.vm.provision :shell, inline: "sudo apt-get update"
  config.vm.provision :shell, inline: "apt-get install -y make build-essential git zsh curl linux-headers-$(uname -r)"
  config.vm.provision :shell, privileged: false, inline: "git clone https://github.com/jo-m/joni-config.git /home/vagrant/.joni-config"
  config.vm.provision :shell, inline: "chsh -s '/usr/bin/zsh' vagrant"
  config.vm.provision :shell, privileged: false, inline: "/home/vagrant/.joni-config/setup.sh"
end
