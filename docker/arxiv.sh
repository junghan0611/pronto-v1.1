#!/bin/bash
cd ..
tar -czvf /tmp/Pronto.tar.gz --exclude ".git" .
cd docker
mv /tmp/Pronto.tar.gz .
exit 0
