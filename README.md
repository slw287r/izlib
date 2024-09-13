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

## Examples

<details>
<summary>Read lines of gzipped or plain file using kstring and izlib</summary>

* code

```c
#include <stdio.h>
#include <stdlib.h>
#include "izlib.h"
#include "kstring.h"

// alias to gzgets with input order swapped
char *kzgets(char *buf, int siz, gzFile fp) { return gzgets(fp, buf, siz); }

int main(int argc, char *argv[])
{
        kstring_t ks = {0, 0, 0};
        gzFile fp = gzopen(argv[1], "r");
        if (!fp)
        {
                fprintf(stderr, "Error openning file [%s]\n", argv[1]);
                exit(EXIT_FAILURE);
        }
        while (kgetline(&ks, (kgets_func *)kzgets, fp) == 0)
        {
                puts(ks.s);
                ks.l = 0;
        }
        if (ks.m)
                free(ks.s);
        gzclose(fp);
}
```

* Compile

```
cc -o a a.c kstring.c -lisal
```

* Run

```
time ./a in.gz | wc -l
```

</details>



## Credits
<a href="http://github.com/wulj2" target="_blank"><img src="https://avatars.githubusercontent.com/u/37930892?v=4" alt="@wulj2" size="32" height="32" width="32" data-view-component="true" class="avatar circle"></a>
<a href="http://github.com/bli25" target="_blank"><img src="https://avatars.githubusercontent.com/u/10385559?v=4" alt="@bli25" size="32" height="32" width="32" data-view-component="true" class="avatar circle"></a>
