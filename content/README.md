# Script content

`framework/` contains the pinned MIT two-player upstream's own scripts and small patch contexts, plus the own player-count probe. This is the content used for the partial single-player trainer component test. See [its provenance](framework/PROVENANCE.txt), [manifest](framework/manifest.json) and [license](framework/LICENSE).

`probe/` contains only the minimal own loader/compiler probe and one small patch context that calls it after the game's player-array initialization. It reports actual script membership; reserved native capacity3 still reports players1 in the single-player test.

Choose one folder as the content root beside an independent test game's executable, named `mw2trio`. Do not merge both folders or copy them over existing content without preserving it first. The [README experiment](../README.md#optional-single-player-component-experiment) uses `framework/`.

Neither folder implements a three-player framework or activates native networking. Quoted game-script context belongs to the game owners; own/mod-authored content follows the retained MIT terms.
