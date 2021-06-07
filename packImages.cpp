#include <QGraphicsItem>
#include "AWindow.h"
#include "ItemValues.h"
#include "binpack2d.h"

using namespace BinPack2D;

typedef QGraphicsItem* APData;
typedef Content<APData>::Vector::iterator ABinPackIter;

void AWindow::packImages()
{
    ContentAccumulator<APData> input;
    ContentAccumulator<APData> output;
    ContentAccumulator<APData> leftover;
    CanvasArray<APData> canvases =
        UniformCanvasArrayBuilder<APData>(1024, 1024, 1).Build();


    // Collect images.
    ItemValues val;
    each_item(gi) {
        if (gi->type() != GIT_PIXMAP)
            continue;
        itemValues(val, gi);

        input += Content<APData>((QGraphicsItem*) gi,
                                Coord(val.x, val.y), Size(val.w, val.h), false);
    }

    // Pack 'em.
    input.Sort();
    bool ok = canvases.Place(input, leftover);
    canvases.CollectContent(output);

    // Update scene.
    ABinPackIter it;
    for (it = output.Get().begin(); it != output.Get().end(); it++) {
        const Content<APData>& con = *it;
        con.content->setPos(con.coord.x, con.coord.y);
    }

    if (! ok) {
        printf( "KR leftover %ld\n", leftover.Get().size());
    }
}
