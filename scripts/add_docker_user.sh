#!/bin/sh
if test "$USER" != "flux"; then
    groupadd -g $UID $USER
    useradd -g $USER -u $UID -d /home/$USER -m $USER
    sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers"
    case "$ID" in
        ubuntu|debian)
            adduser $USER sudo
            ;;
        fedora|rocky|alma|rhel|centos|alpine)
            usermod -G wheel $USER
            ;;
    esac
fi
