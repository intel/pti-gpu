# How to build PTI Library documentation

Our documentation is written in restructured text markup (.rst) and built using [Sphinx](http://www.sphinx-doc.org/en/master/). 

This document explains how to build PTI Library documentation locally. 

## Prerequisites
```
apt install doxygen
pip install -r requirements.txt
```

## Build documentation

Do the following to generate HTML output of the documentation: 

1. Clone PTI repository:

```
git clone https://github.com/intel/pti-gpu
```

2. Go to the `sdk/docs/sphinx` folder:

```
cd sdk/docs/sphinx
```

3. Run in the command line:

```
make html
```


That's it! Your built documentation is located in the ``build/html`` folder. 