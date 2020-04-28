#!/bin/bash

# (1) download data
cd ./data/
wget https://mpetri-data.s3.amazonaws.com/ans/datasets/CCNEWS-RLZ-D64-FLENS.txt.gz &
wget https://mpetri-data.s3.amazonaws.com/ans/datasets/news-docs.2016-WORD-BWTMTF.txt.gz &
wget https://mpetri-data.s3.amazonaws.com/ans/datasets/CCNEWS-RLZ-D64-FOFFSETS.txt.gz &
wget https://mpetri-data.s3.amazonaws.com/ans/datasets/news-docs.2016-WORD.txt.gz &
wait

gzip -d news-docs.2016-WORD-BWTMTF.txt.gz &
gzip -d news-docs.2016-WORD.txt.gz &
gzip -d CCNEWS-RLZ-D64-FLENS.txt.gz &
gzip -d CCNEWS-RLZ-D64-FOFFSETS.txt.gz &
wait

cd ..


mkdir -p ./data/speed
mkdir -p ./data/space

ln -s $(pwd)/data/CCNEWS-RLZ-D64-FLENS.txt ./data/space/02-rlz-d64len.txt
ln -s $(pwd)/data/CCNEWS-RLZ-D64-FOFFSETS.txt ./data/space/06-rlz-d64len.txt
ln -s $(pwd)/data/news-docs.2016-WORD-BWTMTF.txt ./data/space/03-bwtmtf-w.txt
ln -s $(pwd)/data/news-docs.2016-WORD.txt ./data/space/05-newsdocs-w.txt

ln -s $(pwd)/data/CCNEWS-RLZ-D64-FLENS.txt ./data/speed/01-rlz-d64len.txt
ln -s $(pwd)/data/news-docs.2016-WORD.txt ./data/speed/03-newsdocs-w.txt

