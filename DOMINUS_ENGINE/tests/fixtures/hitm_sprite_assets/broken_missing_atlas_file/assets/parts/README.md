Deliberately empty: this fixture tests HitmAssetImporter::Import failing
because `assets/parts/brooklyn_atlas.png` does not exist. This file only
exists so git tracks this otherwise-empty directory (git does not track
empty directories) -- see tests/fixtures/hitm_parts_rig/broken_missing_file/
for the same established pattern.
