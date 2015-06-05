### This tool relies on the following external python modules:
-  `h5py`
-  `poretools` https://github.com/arq5x/poretools
-  `GeneralWriter` which will be part of ngs, but is tempararily included here

`poretools` also relies on `h5py` and `h5py` relies on `hdf5`. `python` will need to be able to locate the `hdf5` library at the time that the `h5py` module is loaded, for example, by setting LD_LIBRARY_PATH:
```
LD_LIBRARY_PATH=/usr/local/hdf5/1.8.10/lib python -O pore-load.py --tmpdir=/dev/shm --output=foo test-files/Ecoli_R7_NONI.tgz
```
Furthermore, to do anything useful, the output of this tool needs to be sent to `general-loader`, which is part of `sra-tools`.
