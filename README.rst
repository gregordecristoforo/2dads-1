2dads
=====
Two dimensional diffusion advection solver.

Integrates a set of coupled diffusion-advection equations of the type
df/dt + u \cdot \div f = NL(u) + kappa \nabla^2 f

with finite difference and spectral methods on the CPU/GPU (cuda).


Directory Layout
----------------

src: Source code
doc: Root directory for sphinx documentation

To build the executable, enter the src directory and build the individual .o targets.
After that build the 2dads target.
A more sophisticated build system would also be nice :D

The documentation of this project can be generated by running sphinx in the root directory.
This requires the cmtinc plugin for sphinx:
https://github.com/w-vi/sphinxcontrib-cmtinc

Enter the doc directory and type
sphinx-build -b html . doc


Contributors
------------
Ralph Kube
Gregor Decristoforo
Odd Erik Garcia

Licensed under the MIT license, 
