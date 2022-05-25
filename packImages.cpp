#include <QComboBox>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QSpinBox>
#include "AWindow.h"
#include "ItemValues.h"
#include "binpack2d.h"
#include "ExtractDialog.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"

enum PackAlgorithm {
    PA_BinPack,
    PA_BinPackSort,
    PA_SkyLine,
    PA_SkyLineBF
};

using namespace BinPack2D;

typedef QGraphicsItem* APData;
typedef Content<APData>::Vector::iterator ABinPackIter;

struct GraphicsItemPackerBP {
    ContentAccumulator<APData> input;
    ContentAccumulator<APData> output;
    ContentAccumulator<APData> leftover;

    void addInput(QGraphicsItem* gi, const ItemValues& val)
    {
        input += Content<APData>(gi, Coord(val.x, val.y),
                                      Size(val.w, val.h), false);
    }

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

struct GraphicsItemPackerSL {
    std::vector<stbrp_rect> input;
    stbrp_context ctx;

    void addInput(int id, int w, int h)
    {
        stbrp_rect rect;
        rect.id = id;
        rect.w = w;
        rect.h = h;
        rect.was_packed = 0;

        input.push_back(rect);
    }

    int pack(int w, int h, bool bestFit)
    {
        stbrp_node* nodes = new stbrp_node[w];
        if (! nodes)
            return 0;

        stbrp_init_target(&ctx, w, h, nodes, w);
        stbrp_setup_heuristic(&ctx,
                        bestFit ? STBRP_HEURISTIC_Skyline_BF_sortHeight
                                : STBRP_HEURISTIC_Skyline_BL_sortHeight);
        int allPacked = stbrp_pack_rects(&ctx, input.data(), input.size());

        delete[] nodes;
        return allPacked;
    }
};

struct GraphicsItemPacker {
    QList<QGraphicsItem *> list;
    GraphicsItemPackerBP* bp;
    GraphicsItemPackerSL* sl;
    int packAlgo;

    void init(int algo) {
        packAlgo = algo;
        if (packAlgo >= PA_SkyLine) {
            bp = NULL;
            sl = new GraphicsItemPackerSL;
        } else {
            bp = new GraphicsItemPackerBP;
            sl = NULL;
        }
    }

    size_t inputCount() const {
        return bp ? bp->input.Get().size() : sl->input.size();
    }

    int packItems(int w, int h,
                  void (*posFunc)(QGraphicsItem*, int, int, void*),
                  void* user) {
        int leftover;
        if (bp) {
            leftover = bp->pack(w, h, (packAlgo == PA_BinPackSort));

            // Update scene.
            ABinPackIter it;
            for (it = bp->output.Get().begin();
                 it != bp->output.Get().end(); it++) {
                const Content<APData>& con = *it;
                posFunc(con.content, con.coord.x, con.coord.y, user);
            }

            delete bp;
        } else {
            sl->pack(w, h, (packAlgo == PA_SkyLineBF));
            leftover = 0;
            for (const auto& it : sl->input) {
                if (it.was_packed)
                    posFunc(list[ it.id ], it.x, it.y, user);
                else
                    ++leftover;
            }
            delete sl;
        }
        return leftover;
    }
};

static void warnIncomplete(QWidget* parent, int leftover)
{
    QMessageBox::warning(parent, "Pack Incomplete",
            QString::number(leftover) + QString(" images did not fit."));
}

static void positionItem(QGraphicsItem* item, int x, int y, void*)
{
    item->setPos(x, y);
}

void AWindow::packImages()
{
    GraphicsItemPacker pk;
    int w, h;
    int leftover;

    pk.init(_packAlgo->currentIndex());

    // Collect images.
    {
    ItemValues val;
    int pad = _packPad->value();

    pk.list = _scene->selectedItems();
    if (pk.list.empty())
        pk.list = _scene->items(Qt::AscendingOrder);

    QList<QGraphicsItem *>::iterator it = pk.list.begin();
    while (it != pk.list.end()) {
        QGraphicsItem* gi = *it;
        if (gi->type() != GIT_PIXMAP) {
            it = pk.list.erase(it);     // Eliminate for snapshot.
            continue;
        }

        itemValues(val, gi);
        val.w += pad;
        val.h += pad;

        if (pk.bp)
            pk.bp->addInput(gi, val);
        else
            pk.sl->addInput(it - pk.list.begin(), val.w, val.h);

        ++it;
    }

    _undo.snapshot(pk.list);
    }

    // Pack 'em.
    if (_docSize.isEmpty()) {
        w = 1024;
        h = 2048;
    } else {
        w = _docSize.width();
        h = _docSize.height();
    }
    leftover = pk.packItems(w, h, positionItem, NULL);

    _undo.commit();

    if (leftover)
        warnIncomplete(this, leftover);
}

struct ExtractRegionData {
    QVector<QGraphicsItem*> removeList;
    QPainter ip;
    QGraphicsPixmapItem* pitem;
};

static void copyRegion(QGraphicsItem* item, int x, int y, void* user)
{
    ExtractRegionData* ed = (ExtractRegionData*) user;
    QGraphicsItem* si = item->parentItem();

    // Copy pixmap region.
    if (si && IS_IMAGE(si)) {
        QPointF pos = item->pos();
        QRectF rect = item->boundingRect();
        QRect srcRect(pos.x(), pos.y(), rect.width(), rect.height());
        ed->ip.drawPixmap(QPoint(x, y),
                          static_cast<QGraphicsPixmapItem*>(si)->pixmap(),
                          srcRect);

        if (ed->removeList.indexOf(si) < 0)
            ed->removeList.push_back(si);
    }

    // Transfer region to new image.
    item->setParentItem(ed->pitem);
    item->setPos(x, y);
}

void AWindow::extractRegionsOp(const QString& file, const QColor& color)
{
    GraphicsItemPacker pk;
    int w, h;
    int leftover;

    pk.init(_packAlgo->currentIndex());

    // Collect regions.
    {
    ItemValues val;
    int pad = _packPad->value();

    pk.list = _scene->selectedItems();
    if (pk.list.empty())
        pk.list = _scene->items(Qt::AscendingOrder);

    QList<QGraphicsItem *>::iterator it = pk.list.begin();
    for (; it != pk.list.end(); ++it) {
        QGraphicsItem* gi = *it;
        if (! IS_REGION(gi))
            continue;

        itemValues(val, gi);
        val.w += pad;
        val.h += pad;

        if (pk.bp)
            pk.bp->addInput(gi, val);
        else
            pk.sl->addInput(it - pk.list.begin(), val.w, val.h);
    }
    }

    if (pk.inputCount() == 0) {
        QMessageBox::warning(this, "Extract Incomplete",
                             "No Regions found; nothing to extract.");
        return;
    }

    // Pack 'em.
    if (_docSize.isEmpty()) {
        w = 1024;
        h = 2048;
    } else {
        w = _docSize.width();
        h = _docSize.height();
    }

    // Create a new image and update the scene.
    {
    QPixmap newPix(w, h);
    ExtractRegionData ed;

    ed.pitem = makeImage(QPixmap(), 0, 0);
    ed.pitem->setData(ID_NAME, file);

    newPix.fill(color);
    ed.ip.begin(&newPix);

    leftover = pk.packItems(w, h, copyRegion, &ed);

    ed.ip.end();
    ed.pitem->setPixmap(newPix);

    // Delete source images from scene.
    removeItems(ed.removeList.constData(), ed.removeList.size());

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
