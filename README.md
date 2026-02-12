# ironOS

ironOS is a hobby operating system project focused on simplicity and minimalism. It uses Nix to ensure a fully reproducible development and compilation environment.

## Prerequisites

You must have Nix installed with Flakes enabled.

## Quickstart

To build the OS and immediately launch it in a QEMU virtual machine:
```
$ nix run
```

To compile the final ISO image without running it (output will be in `./result`):
```
$ nix build
```

## Development

I welcome contributions. To enter a reproducible development shell with all necessary tools pre-installed:
```
$ nix develop
```

## License

This project is licensed under the MIT License.
