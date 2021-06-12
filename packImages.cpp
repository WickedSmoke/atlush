#include <QCheckBox>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QSpinBox>
#include "AWindow.h"
#include "ItemValues.h"
#include "binpack2d.h"
#include "ExtractDialog.h"

using namespace BinPack2D;

typedef QGraphicsItem* APData;
typedef Content<APData>::Vector::iterator ABinPackIter;

struct GraphicsItemPacker {
    ContentAccumulator<APData> input;
    ContentAccumulator<APData> output;
    ContentAccumulator<APData> leftover;

    int pack(int w, int h, bool sort)
    {
        CanvasArray<APData> canvases =
            UniformCanvasArrayBuilder<APData>(w, h, 1).Build();
        if (sort)
            input.Sort();
        canvases.Place(input, leftover);
        canvases.CollectContent(output);
        return leftover.Get().size();
    }
};

static void warnIncomplete(QWidget* parent, int leftover)
{
    QMessageBox::warning(parent, "Pack Incomplete",
            QString::number(leftover) + QString(" images did not fit."));
}

void AWindow::packImages()
{
    GraphicsItemPacker pk;
    int w, h;
    int leftover;

    // Collect images.
    {
    ItemValues val;
    int pad = _packPad->value();

    QList<QGraphicsItem *> list = _scene->selectedItems();
    if (list.empty())
        list = _scene->items(Qt::AscendingOrder);

    for(const QGraphicsItem* gi : list) {
        if (gi->type() != GIT_PIXMAP)
            continue;
        itemValues(val, gi);

        val.w += pad;
        val.h += pad;

        pk.input += Content<APData>((QGraphicsItem*) gi,
                                Coord(val.x, val.y), Size(val.w, val.h), false);
    }
    }

    // Pack 'em.
    if (_docSize.isEmpty())
        w = h = 1024;
    else {
        w = _docSize.width();
        h = _docSize.height();
    }
    leftover = pk.pack(w, h, _packSort->isChecked());

    // Update scene.
    ABinPackIter it;
    for (it = pk.output.Get().begin(); it != pk.output.Get().end(); it++) {
        const Content<APData>& con = *it;
        con.content->setPos(con.coord.x, con.coord.y);
    }

    if (leftover)
        warnIncomplete(this, leftover);
}

void AWindow::extractRegionsOp(const QString& file, const QColor& color)
{
    GraphicsItemPacker pk;
    int w, h;
    int leftover;


    // Collect regions.
    {
    ItemValues val;
    int pad = _packPad->value();

    QList<QGraphicsItem *> list = _scene->selectedItems();
    if (list.empty())
        list = _scene->items(Qt::AscendingOrder);

    for(const QGraphicsItem* gi : list) {
        if (! IS_REGION(gi))
            continue;
        itemValues(val, gi);

        val.w += pad;
        val.h += pad;

        pk.input += Content<APData>((QGraphicsItem*) gi,
                                Coord(val.x, val.y), Size(val.w, val.h), false);
    }
    }

    if (pk.input.Get().empty()) {
        QMessageBox::warning(this, "Extract Incomplete",
                             "No Regions found; nothing to extract.");
        return;
    }

    // Pack 'em.
    if (_docSize.isEmpty())
        w = h = 1024;
    else {
        w = _docSize.width();
        h = _docSize.height();
    }
    leftover = pk.pack(w, h, _packSort->isChecked());

    // Create a new image and update the scene.
    {
    QVector<QGraphicsItem*> removeList;
    QPixmap newPix(w, h);
    QPainter ip;
    QRect srcRect;
    ABinPackIter it;
    QGraphicsItem* si;
    QGraphicsItem* gi;
    QGraphicsPixmapItem* pitem;

    pitem = makeImage(QPixmap(), 0, 0);
    pitem->setData(ID_NAME, file);

    newPix.fill(color);
    ip.begin(&newPix);
    for (it = pk.output.Get().begin(); it != pk.output.Get().end(); it++) {
        const Content<APData>& con = *it;
        gi = con.content;
        si = gi->parentItem();

        // Copy pixmap region.
        if (si && IS_IMAGE(si)) {
            QPointF pos = gi->pos();
            QRectF rect = gi->boundingRect();
            srcRect.setRect(pos.x(), pos.y(), rect.width(), rect.height());
            ip.drawPixmap(QPoint(con.coord.x, con.coord.y),
                          static_cast<QGraphicsPixmapItem*>(si)->pixmap(),
                          srcRect);

            if (removeList.indexOf(si) < 0)
                removeList.push_back(si);
        }

        // Transfer region to new image.
        gi->setParentItem(pitem);
        gi->setPos(con.coord.x, con.coord.y);
    }
    ip.end();
    pitem->setPixmap(newPix);

    // Delete source images from scene.
    removeItems(removeList.constData(), removeList.size());

    if (! newPix.save(file)) {
        QString error("Could not save image to file ");
        QMessageBox::warning(this, "Image Save Error", error + file);
    }
    }

    if (leftover)
        warnIncomplete(this, leftover);
}

void AWindow::extractRegions()
{
    QString file;
    QColor color;

    ExtractDialog* dlg = new ExtractDialog(this);
    if (dlg->exec() == QDialog::Accepted) {
        file  = dlg->file();
        color = dlg->color();
    }
    delete dlg;

    if (! file.isEmpty())
        extractRegionsOp(file, color);
}
