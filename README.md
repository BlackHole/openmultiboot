# SmartMultiBoot - 1.0 (c) 2019 Eddi De Pieri

# Features:

# preserve naming of openmultiboot - existing stick will still boot

# TODO: 
implement cpio/kexec preparation
mount -o bind /dev/mmcblk1 /omb/.kernel/current.kernel to protect flash kernel from rewrite
save blkid to config to continue boot if missing stick (https://stackoverflow.com/questions/6748429/using-libblkid-to-find-uuid-of-a-partition)
save config to flash


This is a fork of OpenMultiboot - For original credits look at https://github.com/oe-alliance/openmultiboot

# OpenMultiBoot - 1.0 2014/10/03#

- OpenMultiBoot will be donated to OE-Alliance by GigaBlue.
- OpenMultiBoot can be modified or adapted to work with stb's from other vendors as long naming "openMultiBoot" is not changed and logos of openMultiBoot persists.
- OpenMultiBoot uses boxbranding and informations from oe-alliance bitbake recipes. Due to that - ipk's are specific for each stb! Do not try to use OpenMultiBoot from stb A on stb B.
- **GigaBlue takes no responsibility for any potential damage openMultiBoot might cause on stb's from other vendors.**
- OpenMultiBoot requires some kernel-modules to work. They will be installed on installation of OpenMultiBoot.
    - **ubifs** - **kernel-module-nandsim**
    - **jffs2** - **kernel-module-nandsim kernel-module-block2mtd**

(c) 2014 Impex-Sat GmbH & Co. KG - http://www.gigablue.de


## Notes: ##
- < device > has to be formated in ext4
- < device > should be a fast usb-stick or ssd - not recommended to use a normal hdd as hdd would almost never stop spinning
- Upload any zipped image to < device >/open-multiboot-upload or /omb/open-multiboot-upload
- Zipped image needs regular folderstructure like flashing from usb-device
- It is not recommended to re-use existing settings - do not blame us if problems occur
- Check your recording path configuration and existing timers if you attach a new device
- Do not use Flash-Online if you have not booted your regularly flashed image - use delete/install in OpenMultiBoot of regularly flashed image
- Special thx goes to skaman, kajgan, captain, arn354 and all testers!

[![Play openMultiBoot YouTube](http://img.youtube.com/vi/WYOYCraLoMk/0.jpg)](https://www.youtube.com/watch?v=WYOYCraLoMk)
