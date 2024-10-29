# Dear ImGui Direct2D Backend implementation

## Changelog

* 2023-11-29: feat: working triangle rendering
* 2024-01-10: refactor: simplification of polygon detection (bit buggy on gradients)
* 2024-01-11: fix: correct gradient detection, disable anti-aliasing flags due to rendring bug
* 2024-10-29: feat: text rendering

For more information see [WIKI](https://github.com/rymut/imgui_impl_d2d/wiki).

## Building

This project uses [Conan 2.0](https://github.com/conan-io/conan) package manager to get ImGui library & required example files.

Openning directory under Microsoft Visual Studio 2019 should generate whole project & download required libraries & assets from conan center.

## License

This software is licensed under MIT License, see [LICENSE](https://github.com/rymut/imgui_impl_d2d/blob/master/LICENSE) for more information
