# Third Party Projects

## File structure

```
third-party/
├── compute-runtime/
│   └── third-party.txt
├── ../
│   └── third-party.txt
└── some-3rd-party-project/
    └── third-party.txt
```

## Info

Contains vendored third party projects used by Intel(R) PTI that are not
practical to be used by CMake's `FetchContent`. `FetchContent` is typically the
preferred way to consume third party projects.
