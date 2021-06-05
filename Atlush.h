#ifndef ATLUSH_H
#define ATLUSH_H

#define APP_NAME    "Atlush"
#define APP_VERSION "0.0.1"

#include <vector>
#include <QString>

struct IARegion {
    QString name;
    int x, y, w, h;

    void setArea(int a, int b, int c, int d) {
        x = a;
        y = b;
        w = c;
        h = d;
    }
};

struct IAGroup : public IARegion {
    std::vector<IARegion> children;
};

#endif  // ATLUSH_H
