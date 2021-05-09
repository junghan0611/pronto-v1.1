#!/bin/bash
apt-get update
apt-get -y install apt-utils
apt-get -y install openssh-server

mkdir ~/.ssh
touch ~/.ssh/known_hosts
ssh-keyscan github.com >> ~/.ssh/known_hosts

cd /app
mkdir Pronto
cd Pronto
tar -xzvf ../Pronto.tar.gz
rm ../Pronto.tar.gz

./dependencies.sh

# Build
cd /app/Pronto/src && make
cd ..
sed -i 's/sudo //g' init_ext4.sh
cd motivation/pmemkv-0.3x-vanilla
cp ../pmemkv_throughput.cc src/
make example
cd ../../benchmark
make

# Clean-up test scripts
sed -i 's/sudo //g' run-perf.sh
sed -i 's/LD_LIBRARY_PATH=/LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/g' run-perf.sh
sed -i 's/sudo //g' run-recovery.sh
sed -i 's/LD_LIBRARY_PATH=/LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/g' run-recovery.sh
sed -i 's/sudo //g' sensitivity/run.sh

exit 0
