#include <QGraphicsItem>
#include <QMessageBox>
#include <QSpinBox>
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

    int w, h;
    if (_docSize.isEmpty())
        w = h = 1024;
    else {
        w = _docSize.width();
        h = _docSize.height();
    }
    CanvasArray<APData> canvases =
        UniformCanvasArrayBuilder<APData>(w, h, 1).Build();


    // Collect images.
    ItemValues val;
    int pad = _packPad->value();
    each_item(gi) {
        if (gi->type() != GIT_PIXMAP)
            continue;
        itemValues(val, gi);

        val.w += pad;
        val.h += pad;

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
        QMessageBox::warning(this, "Pack Incomplete",
                QString::number(leftover.Get().size()) +
                QString(" images did not fit."));
    }
}
