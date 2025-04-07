#!/bin/sh
if test "$USER" != "flux"; then
    sudo groupadd -g $UID $USER
    sudo useradd -g $USER -u $UID -d /home/$USER -m $USER
    sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers"
    case "$ID" in
        ubuntu|debian)
            sudo adduser $USER sudo
            ;;
        fedora|rocky|alma|rhel|centos|alpine)
            sudo usermod -G wheel $USER
            ;;
    esac
fi
