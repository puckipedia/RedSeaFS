cd filesystem
make
cp objects.*-debug/redseafs ~/config/non-packaged/add-ons/userlandfs
sleep 1
mkdir -p /mnt
mount -t userlandfs -p redseafs /dev/disk/ata/0/slave/0 /mnt
