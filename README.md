# izlib
Drop in replacement for zlib.h by isa-l.

## Usage

* Install [Intel(R) Intelligent Storage Acceleration Library](https://github.com/intel/isa-l#intelr-intelligent-storage-acceleration-library)
* Replace "zlib.h" with "izlib.h" and re-compile your file ([seqtk](https://github.com/lh3/seqtk) for example)

| Type | Files | Include | Link flag |
|------|-------|---------|--------------|
| zlib | `zlib.h` | "zlib.h" | `-lz` |
| izlib| `izlib.h`, `igzip_lib.h` (from isa-l) | "izlib.h" | `-lisal` |

## Benchmark

* Data

|Reads       |    Bases       |   Q20_bases    |   Q30_bases    |   GC_bases  |  File size (`toy.fq.gz`)|
|------------|------------|------------|------------|------------|------------|
|59,581,851    |    2,979,092,550  |   2,899,802,990   |   2,710,969,020   |   1,188,643,406  |  2,942,110,279 |


* Command

```
time seqtk seq -FE toy.fq.gz > /dev/null
```

* Results

| `seqtk` version | zlib       | izlib      |  speed up |
|-------|-------------|------------|--------------------|
| Runtime  |  1m18.050s  |  0m23.648s | 3.3×  |

## Caveats
Isa-l's `igzip` program sometimes complains about big concatenated gzip file. I'm not sure whether the issue resists in the above API. **Use at your own risk**.

## Credits
<a href="http://github.com/wulj2"><img src="https://avatars.githubusercontent.com/u/37930892?v=4" alt="@wulj2" size="32" height="32" width="32" data-view-component="true" class="avatar circle"></a>
<a href="http://github.com/bli25"><img src="https://avatars.githubusercontent.com/u/10385559?v=4" alt="@bli25" size="32" height="32" width="32" data-view-component="true" class="avatar circle"></a>