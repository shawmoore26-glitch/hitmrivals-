Deliberately empty of `parts.json` — see `../README.md`. This file exists
only so git tracks the directory itself (git does not track empty
directories); its presence is not read by `HitmPartsRig::Import`, which
looks for `parts.json` specifically and does not exist in this directory.
