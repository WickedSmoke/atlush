/*
    Image Atlas Reader
    Version 1.0
*/

enum AtlSyntaxElement {
    ATL_DOCUMENT,
    ATL_IMAGE,
    ATL_GROUP_BEGIN,
    ATL_GROUP_END,
    ATL_REGION
};

struct AtlRegion {
    const char* projPath;
    const char* name;
    int x, y, w, h;
    int hotx, hoty;
};


#include <stdio.h>
#include <stdlib.h>

static int atl_isRelativePath(const char* path)
{
#ifdef _WIN32
    // NOTE: "C:foo" is relative to the current directory on C:.
    return (path[0] != '\\' && path[1] != ':');
#else
    return path[0] != '/';
#endif
}

// Return number of characters in the path up to the filename, or zero
// if no path separator is found.
static int atl_path(const char* path)
{
    if (path[0]) {
        const char* cp = path;
        while (*cp)
            ++cp;

        while (1) {
            --cp;
#ifdef _WIN32
            if (*cp == '\\' || *cp == '/')
#else
            if (*cp == '/')
#endif
                return (int) (cp - path) + 1;
            if (cp == path)
                break;
        }
    }
    return 0;
}

/*
 * Read items from an image-atlas file.
 *
 * Return zero on error.  In this case errorLine is set to either the line
 * number where parsing failed or -1 on a file open or read error.
 */
int atl_read(const char* path, int* errorLine,
             void (*element)(int, const AtlRegion* reg, void*), void* user)
{
    int done = 0;
    FILE* fp = fopen(path, "r");
    if (! fp) {
        *errorLine = -1;
        return 0;
    }

    {
    AtlRegion reg;
    const size_t bufSize = 1000;
    const char* itemPattern;
    char* imagePath = NULL;
    char* buf = (char*) malloc(bufSize);
    int nested = 0;
    int lineCount = 0;
    int pathLen = atl_path(path);
    char nextCh;
    int ch;

    reg.name = buf;

    // Parse Boron string!/coord!/block! values.
    while (fread(buf, 1, 1, fp) == 1) {
next_buf0:
      switch (buf[0]) {
        case '"':
            itemPattern = "%999[^\"]\" %d,%d,%d,%d%c";
get_item:
            if (fscanf(fp, itemPattern,
                       buf, &reg.x, &reg.y, &reg.w, &reg.h, &nextCh) != 6)
                goto fail;
            if (nextCh == ',') {
                if (fscanf(fp, "%d,%d", &reg.hotx, &reg.hoty) != 2)
                    goto fail;
                nextCh = 0;
            } else
                reg.hotx = reg.hoty = 0;
            //printf("KR %s %d,%d,%d,%d\n", buf, reg.x, reg.y, reg.w, reg.h);

            if (nested) {
                reg.projPath = NULL;
                reg.name = buf;
                element(ATL_REGION, &reg, user);
            } else {
                if (atl_isRelativePath(buf)) {
                    if (! imagePath) {
                        imagePath = (char*) malloc(bufSize + pathLen);
                        memcpy(imagePath, path, pathLen);
                    }
                    strcpy(imagePath + pathLen, buf);
                    reg.projPath = imagePath;
                } else
                    reg.projPath = buf;

                reg.name = buf;
                element(ATL_IMAGE, &reg, user);
            }

            if (nextCh) {
                buf[0] = nextCh;
                goto next_buf0;
            }
            break;

        case '[':
            ++nested;
            element(ATL_GROUP_BEGIN, NULL, user);
            break;

        case ']':
            --nested;
            element(ATL_GROUP_END, NULL, user);
            break;

        case 'i':
            if (fscanf(fp, "mage-atlas %d %d,%d", &reg.x, &reg.w, &reg.h) != 3)
                goto fail;
            if (reg.x != 1)
                goto fail;          // Invalid version.

            reg.projPath = path;
            reg.name = NULL;
            reg.y = 0;
            element(ATL_DOCUMENT, &reg, user);
            break;

        case '{':
            itemPattern = "%999[^}]} %d,%d,%d,%d%c";
            goto get_item;

        case ';':
            while ((ch = fgetc(fp)) != EOF) {
                if (ch == '\n')
                    break;
            }
            break;

        case ' ':
        case '\t':
            break;

        case '\n':
            ++lineCount;
            break;

        default:
            goto fail;
      }
    }
    done = 1;

fail:
    *errorLine = lineCount;
    free(imagePath);
    free(buf);
    }

    fclose(fp);
    return done;
}
